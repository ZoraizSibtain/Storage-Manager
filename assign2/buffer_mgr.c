#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buffer_mgr.h"
#include "storage_mgr.h"

/* Forward declarations of page replacement strategy functions */
static void FIFO(BM_BufferPool *const bm, FrameInfo *page);
static void LRU(BM_BufferPool *const bm, FrameInfo *page);
static void CLOCK(BM_BufferPool *const bm, FrameInfo *page);

/* Helper function to get buffer pool info */
static inline BufferPoolInfo* getPoolInfo(BM_BufferPool *const bm) {
    return (BufferPoolInfo*)bm->mgmtData;
}

/*
 * Initializes a new buffer pool
 * Creates a buffer pool with the specified number of page frames
 * @param bm - Pointer to buffer pool structure
 * @param pageFileName - Name of the page file to manage
 * @param numPages - Number of page frames in the buffer pool
 * @param strategy - Page replacement strategy to use
 * @param stratData - Strategy-specific data (unused)
 * @return RC_OK on success, error code otherwise
 */
extern RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                         const int numPages, ReplacementStrategy strategy, void *stratData)
{
    if (bm == NULL) {
        return RC_BUFF_POOL_NOT_FOUND;
    }

    if (pageFileName == NULL) {
        return RC_FILE_NOT_FOUND;
    }

    if (numPages <= 0) {
        return RC_ERROR;
    }

    /* Allocate and initialize buffer pool info structure */
    BufferPoolInfo *poolInfo = (BufferPoolInfo*)malloc(sizeof(BufferPoolInfo));
    if (poolInfo == NULL) {
        return RC_ERROR;
    }

    /* Allocate page frames */
    poolInfo->frames = (FrameInfo*)calloc(numPages, sizeof(FrameInfo));
    if (poolInfo->frames == NULL) {
        free(poolInfo);
        return RC_ERROR;
    }

    /* Initialize all frames */
    for (int i = 0; i < numPages; i++) {
        poolInfo->frames[i].pageNumber = NO_PAGE;
        poolInfo->frames[i].dirtybit = 0;
        poolInfo->frames[i].accessCount = 0;
        poolInfo->frames[i].secondChance = 0;
        poolInfo->frames[i].recentHit = 0;
        poolInfo->frames[i].index = 0;
        poolInfo->frames[i].data = NULL;
    }

    /* Initialize buffer pool metadata */
    poolInfo->readCount = 0;
    poolInfo->writeCount = 0;
    poolInfo->recentHitCount = 0;
    poolInfo->frameIndex = 0;
    poolInfo->clockPointer = 0;
    poolInfo->bufferSize = numPages;

    /* Set buffer pool attributes */
    bm->pageFile = (char*)malloc(strlen(pageFileName) + 1);
    if (bm->pageFile == NULL) {
        free(poolInfo->frames);
        free(poolInfo);
        return RC_ERROR;
    }
    strcpy(bm->pageFile, pageFileName);

    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = poolInfo;

    return RC_OK;
}

/*
 * Shuts down the buffer pool
 * Writes all dirty pages to disk and frees all resources
 * @param bm - Pointer to buffer pool
 * @return RC_OK on success, error code otherwise
 */
extern RC shutdownBufferPool(BM_BufferPool *const bm)
{
    if (bm == NULL) {
        return RC_BUFF_POOL_NOT_FOUND;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return RC_ERROR;
    }

    /* Flush all dirty pages */
    RC result = forceFlushPool(bm);
    if (result != RC_OK) {
        return result;
    }

    /* Check for pinned pages */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].accessCount != 0) {
            return RC_PINNED_PAGES_IN_BUFFER;
        }
    }

    /* Free all frame data */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].data != NULL) {
            free(poolInfo->frames[i].data);
        }
    }

    /* Free pool resources */
    free(poolInfo->frames);
    free(poolInfo);
    free(bm->pageFile);

    bm->mgmtData = NULL;

    return RC_OK;
}

/*
 * Writes all dirty pages (not pinned) to disk
 * @param bm - Pointer to buffer pool
 * @return RC_OK on success, error code otherwise
 */
extern RC forceFlushPool(BM_BufferPool *const bm)
{
    if (bm == NULL) {
        return RC_BUFF_POOL_NOT_FOUND;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return RC_ERROR;
    }

    SM_FileHandle fh;
    if (openPageFile(bm->pageFile, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }

    /* Write all dirty, unpinned pages */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].dirtybit && poolInfo->frames[i].accessCount == 0) {
            if (writeBlock(poolInfo->frames[i].pageNumber, &fh,
                          poolInfo->frames[i].data) != RC_OK) {
                closePageFile(&fh);
                return RC_WRITE_FAILED;
            }
            poolInfo->frames[i].dirtybit = 0;
            poolInfo->writeCount++;
        }
    }

    closePageFile(&fh);
    return RC_OK;
}

/*
 * Marks a page as dirty
 * @param bm - Pointer to buffer pool
 * @param page - Page handle to mark as dirty
 * @return RC_OK on success, error code otherwise
 */
extern RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    if (bm == NULL) {
        return RC_BUFF_POOL_NOT_FOUND;
    }

    if (page == NULL) {
        return RC_ERROR;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return RC_ERROR;
    }

    /* Find and mark the page as dirty */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].pageNumber == page->pageNum) {
            poolInfo->frames[i].dirtybit = 1;
            return RC_OK;
        }
    }

    return RC_ERROR;
}

/*
 * Unpins a page, decrementing its pin count
 * @param bm - Pointer to buffer pool
 * @param page - Page handle to unpin
 * @return RC_OK on success
 */
extern RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    if (bm == NULL) {
        return RC_BUFF_POOL_NOT_FOUND;
    }

    if (page == NULL) {
        return RC_ERROR;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return RC_ERROR;
    }

    /* Find and unpin the page */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].pageNumber == page->pageNum) {
            if (poolInfo->frames[i].accessCount > 0) {
                poolInfo->frames[i].accessCount--;
            }
            break;
        }
    }

    return RC_OK;
}

/*
 * Forces a specific page to be written to disk
 * @param bm - Pointer to buffer pool
 * @param page - Page handle to write
 * @return RC_OK on success, error code otherwise
 */
extern RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    if (bm == NULL) {
        return RC_BUFF_POOL_NOT_FOUND;
    }

    if (page == NULL) {
        return RC_ERROR;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return RC_ERROR;
    }

    SM_FileHandle fh;
    if (openPageFile(bm->pageFile, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }

    /* Find and write the page */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].pageNumber == page->pageNum) {
            if (writeBlock(poolInfo->frames[i].pageNumber, &fh,
                          poolInfo->frames[i].data) != RC_OK) {
                closePageFile(&fh);
                return RC_WRITE_FAILED;
            }
            poolInfo->frames[i].dirtybit = 0;
            poolInfo->writeCount++;
            break;
        }
    }

    closePageFile(&fh);
    return RC_OK;
}

/*
 * Pins a page in the buffer pool
 * Loads the page from disk if not already in buffer
 * @param bm - Pointer to buffer pool
 * @param page - Page handle to populate
 * @param pageNum - Page number to pin
 * @return RC_OK on success, error code otherwise
 */
extern RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
    if (bm == NULL) {
        return RC_BUFF_POOL_NOT_FOUND;
    }

    if (page == NULL) {
        return RC_ERROR;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return RC_ERROR;
    }

    SM_FileHandle fh;
    if (openPageFile(bm->pageFile, &fh) != RC_OK) {
        return RC_FILE_NOT_FOUND;
    }

    /* Check if page is already in buffer */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].pageNumber == pageNum) {
            poolInfo->frames[i].accessCount++;
            poolInfo->recentHitCount++;

            if (bm->strategy == RS_CLOCK) {
                poolInfo->frames[i].secondChance = 1;
            } else if (bm->strategy == RS_LRU) {
                poolInfo->frames[i].recentHit = poolInfo->recentHitCount;
            }

            page->pageNum = pageNum;
            page->data = poolInfo->frames[i].data;
            closePageFile(&fh);
            return RC_OK;
        }
    }

    /* Page not in buffer - find empty frame or use replacement strategy */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].pageNumber == NO_PAGE) {
            /* Found empty frame */
            poolInfo->frames[i].data = (char*)malloc(PAGE_SIZE);
            if (poolInfo->frames[i].data == NULL) {
                closePageFile(&fh);
                return RC_ERROR;
            }

            ensureCapacity(pageNum + 1, &fh);
            if (readBlock(pageNum, &fh, poolInfo->frames[i].data) != RC_OK) {
                free(poolInfo->frames[i].data);
                poolInfo->frames[i].data = NULL;
                closePageFile(&fh);
                return RC_READ_NON_EXISTING_PAGE;
            }

            poolInfo->frames[i].pageNumber = pageNum;
            poolInfo->frames[i].accessCount = 1;
            poolInfo->frames[i].dirtybit = 0;
            poolInfo->frames[i].index = 0;
            poolInfo->readCount++;
            poolInfo->recentHitCount++;

            if (bm->strategy == RS_CLOCK) {
                poolInfo->frames[i].secondChance = 0;
            } else if (bm->strategy == RS_LRU) {
                poolInfo->frames[i].recentHit = poolInfo->recentHitCount;
            }

            page->pageNum = pageNum;
            page->data = poolInfo->frames[i].data;
            closePageFile(&fh);
            return RC_OK;
        }
    }

    /* No empty frame - use replacement strategy */
    FrameInfo *newFrame = (FrameInfo*)malloc(sizeof(FrameInfo));
    if (newFrame == NULL) {
        closePageFile(&fh);
        return RC_ERROR;
    }

    newFrame->data = (char*)malloc(PAGE_SIZE);
    if (newFrame->data == NULL) {
        free(newFrame);
        closePageFile(&fh);
        return RC_ERROR;
    }

    ensureCapacity(pageNum + 1, &fh);
    if (readBlock(pageNum, &fh, newFrame->data) != RC_OK) {
        free(newFrame->data);
        free(newFrame);
        closePageFile(&fh);
        return RC_READ_NON_EXISTING_PAGE;
    }

    newFrame->pageNumber = pageNum;
    newFrame->accessCount = 1;
    newFrame->dirtybit = 0;
    newFrame->index = 0;
    poolInfo->readCount++;
    poolInfo->recentHitCount++;

    if (bm->strategy == RS_CLOCK) {
        newFrame->secondChance = 0;
    } else if (bm->strategy == RS_LRU) {
        newFrame->recentHit = poolInfo->recentHitCount;
    }

    page->pageNum = pageNum;
    page->data = newFrame->data;

    /* Apply replacement strategy */
    switch (bm->strategy) {
        case RS_FIFO:
            FIFO(bm, newFrame);
            break;
        case RS_LRU:
            LRU(bm, newFrame);
            break;
        case RS_CLOCK:
            CLOCK(bm, newFrame);
            break;
        default:
            free(newFrame->data);
            free(newFrame);
            closePageFile(&fh);
            return RC_ERROR;
    }

    free(newFrame);
    closePageFile(&fh);
    return RC_OK;
}

/*
 * FIFO page replacement strategy
 * Replaces the oldest page in the buffer
 * @param bm - Pointer to buffer pool
 * @param page - New page to insert
 */
static void FIFO(BM_BufferPool *const bm, FrameInfo *page)
{
    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return;
    }

    /* Find next frame to replace */
    for (int attempts = 0; attempts < poolInfo->bufferSize; attempts++) {
        int idx = poolInfo->frameIndex;

        if (poolInfo->frames[idx].accessCount == 0) {
            /* Frame can be replaced */
            if (poolInfo->frames[idx].dirtybit) {
                SM_FileHandle fh;
                if (openPageFile(bm->pageFile, &fh) == RC_OK) {
                    writeBlock(poolInfo->frames[idx].pageNumber, &fh,
                              poolInfo->frames[idx].data);
                    closePageFile(&fh);
                    poolInfo->writeCount++;
                }
            }

            /* Replace frame */
            free(poolInfo->frames[idx].data);
            poolInfo->frames[idx].data = page->data;
            poolInfo->frames[idx].pageNumber = page->pageNumber;
            poolInfo->frames[idx].dirtybit = page->dirtybit;
            poolInfo->frames[idx].accessCount = page->accessCount;

            poolInfo->frameIndex = (poolInfo->frameIndex + 1) % poolInfo->bufferSize;
            return;
        }

        poolInfo->frameIndex = (poolInfo->frameIndex + 1) % poolInfo->bufferSize;
    }
}

/*
 * LRU page replacement strategy
 * Replaces the least recently used unpinned page
 * @param bm - Pointer to buffer pool
 * @param page - New page to insert
 */
static void LRU(BM_BufferPool *const bm, FrameInfo *page)
{
    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return;
    }

    int replaceIdx = -1;
    int leastRecentHit = poolInfo->recentHitCount + 1;

    /* Find least recently used unpinned frame */
    for (int i = 0; i < poolInfo->bufferSize; i++) {
        if (poolInfo->frames[i].accessCount == 0) {
            if (poolInfo->frames[i].recentHit < leastRecentHit) {
                leastRecentHit = poolInfo->frames[i].recentHit;
                replaceIdx = i;
            }
        }
    }

    if (replaceIdx == -1) {
        return; /* All frames are pinned */
    }

    /* Write dirty page if needed */
    if (poolInfo->frames[replaceIdx].dirtybit) {
        SM_FileHandle fh;
        if (openPageFile(bm->pageFile, &fh) == RC_OK) {
            writeBlock(poolInfo->frames[replaceIdx].pageNumber, &fh,
                      poolInfo->frames[replaceIdx].data);
            closePageFile(&fh);
            poolInfo->writeCount++;
        }
    }

    /* Replace frame */
    free(poolInfo->frames[replaceIdx].data);
    poolInfo->frames[replaceIdx].data = page->data;
    poolInfo->frames[replaceIdx].pageNumber = page->pageNumber;
    poolInfo->frames[replaceIdx].dirtybit = page->dirtybit;
    poolInfo->frames[replaceIdx].accessCount = page->accessCount;
    poolInfo->frames[replaceIdx].recentHit = page->recentHit;
}

/*
 * CLOCK page replacement strategy
 * Uses second-chance algorithm to select victim page
 * @param bm - Pointer to buffer pool
 * @param page - New page to insert
 */
static void CLOCK(BM_BufferPool *const bm, FrameInfo *page)
{
    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return;
    }

    /* Sweep through frames looking for victim */
    int attempts = 0;
    int maxAttempts = poolInfo->bufferSize * 2; /* Prevent infinite loop */

    while (attempts < maxAttempts) {
        int idx = poolInfo->clockPointer;

        if (poolInfo->frames[idx].accessCount == 0) {
            if (poolInfo->frames[idx].secondChance == 0) {
                /* Found victim */
                if (poolInfo->frames[idx].dirtybit) {
                    SM_FileHandle fh;
                    if (openPageFile(bm->pageFile, &fh) == RC_OK) {
                        writeBlock(poolInfo->frames[idx].pageNumber, &fh,
                                  poolInfo->frames[idx].data);
                        closePageFile(&fh);
                        poolInfo->writeCount++;
                    }
                }

                /* Replace frame */
                free(poolInfo->frames[idx].data);
                poolInfo->frames[idx].data = page->data;
                poolInfo->frames[idx].pageNumber = page->pageNumber;
                poolInfo->frames[idx].dirtybit = page->dirtybit;
                poolInfo->frames[idx].accessCount = page->accessCount;
                poolInfo->frames[idx].secondChance = 0;

                poolInfo->clockPointer = (poolInfo->clockPointer + 1) % poolInfo->bufferSize;
                return;
            } else {
                /* Give second chance */
                poolInfo->frames[idx].secondChance = 0;
            }
        }

        poolInfo->clockPointer = (poolInfo->clockPointer + 1) % poolInfo->bufferSize;
        attempts++;
    }
}

/*
 * Returns array of page numbers currently in buffer pool
 * @param bm - Pointer to buffer pool
 * @return Array of page numbers (NO_PAGE for empty frames)
 */
extern PageNumber *getFrameContents(BM_BufferPool *const bm)
{
    if (bm == NULL) {
        return NULL;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return NULL;
    }

    PageNumber *contents = (PageNumber*)malloc(sizeof(PageNumber) * poolInfo->bufferSize);
    if (contents == NULL) {
        return NULL;
    }

    for (int i = 0; i < poolInfo->bufferSize; i++) {
        contents[i] = (poolInfo->frames[i].pageNumber == NO_PAGE) ?
                      NO_PAGE : poolInfo->frames[i].pageNumber;
    }

    return contents;
}

/*
 * Returns array of dirty flags for all frames
 * @param bm - Pointer to buffer pool
 * @return Array of boolean dirty flags
 */
extern bool *getDirtyFlags(BM_BufferPool *const bm)
{
    if (bm == NULL) {
        return NULL;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return NULL;
    }

    bool *dirtyFlags = (bool*)malloc(sizeof(bool) * poolInfo->bufferSize);
    if (dirtyFlags == NULL) {
        return NULL;
    }

    for (int i = 0; i < poolInfo->bufferSize; i++) {
        dirtyFlags[i] = (poolInfo->frames[i].dirtybit == 1);
    }

    return dirtyFlags;
}

/*
 * Returns array of fix counts (pin counts) for all frames
 * @param bm - Pointer to buffer pool
 * @return Array of fix counts
 */
extern int *getFixCounts(BM_BufferPool *const bm)
{
    if (bm == NULL) {
        return NULL;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return NULL;
    }

    int *fixCounts = (int*)malloc(sizeof(int) * poolInfo->bufferSize);
    if (fixCounts == NULL) {
        return NULL;
    }

    for (int i = 0; i < poolInfo->bufferSize; i++) {
        fixCounts[i] = (poolInfo->frames[i].accessCount > 0) ?
                       poolInfo->frames[i].accessCount : 0;
    }

    return fixCounts;
}

/*
 * Returns the number of pages read from disk since initialization
 * @param bm - Pointer to buffer pool
 * @return Number of read I/O operations
 */
extern int getNumReadIO(BM_BufferPool *const bm)
{
    if (bm == NULL) {
        return 0;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return 0;
    }

    return poolInfo->readCount;
}

/*
 * Returns the number of pages written to disk since initialization
 * @param bm - Pointer to buffer pool
 * @return Number of write I/O operations
 */
extern int getNumWriteIO(BM_BufferPool *const bm)
{
    if (bm == NULL) {
        return 0;
    }

    BufferPoolInfo *poolInfo = getPoolInfo(bm);
    if (poolInfo == NULL) {
        return 0;
    }

    return poolInfo->writeCount;
}
