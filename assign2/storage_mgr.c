#include "dberror.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "storage_mgr.h"

/*
 * Initializes the storage manager
 * This function can be used to perform any one-time initialization required
 * by the storage manager module
 */
extern void initStorageManager(void)
{
    /* No initialization required for basic implementation */
}

/*
 * Creates a new page file with the given filename
 * The initial file contains one page filled with zero bytes
 * @param fileName - Name of the file to create
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file creation fails
 */
extern RC createPageFile(const char *fileName)
{
    FILE *filePtr = fopen(fileName, "w+");
    if (filePtr == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }

    /* Allocate memory for one page initialized to zero */
    SM_PageHandle newPage = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    if (newPage == NULL)
    {
        fclose(filePtr);
        return RC_WRITE_FAILED;
    }

    /* Write the empty page to file */
    size_t bytesWritten = fwrite(newPage, sizeof(char), PAGE_SIZE, filePtr);

    /* Clean up resources */
    fclose(filePtr);
    free(newPage);

    if (bytesWritten < PAGE_SIZE)
    {
        return RC_WRITE_FAILED;
    }

    return RC_OK;
}

/*
 * Opens an existing page file
 * Populates the file handle with file information including total pages
 * @param fileName - Name of the file to open
 * @param fHandle - Pointer to file handle structure to populate
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist,
 *         RC_READ_NON_EXISTING_PAGE if file operations fail
 */
extern RC openPageFile(const char *fileName, SM_FileHandle *fHandle)
{
    FILE *filePtr = fopen(fileName, "r");
    if (filePtr == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }

    /* Set file metadata in handle */
    fHandle->fileName = (char *)fileName;
    fHandle->curPagePos = 0;

    /* Seek to end to determine file size */
    if (fseek(filePtr, 0L, SEEK_END) != 0)
    {
        fclose(filePtr);
        return RC_READ_NON_EXISTING_PAGE;
    }

    long fileSize = ftell(filePtr);
    if (fileSize == -1)
    {
        fclose(filePtr);
        return RC_READ_NON_EXISTING_PAGE;
    }

    /* Calculate total number of pages */
    fHandle->totalNumPages = (fileSize % PAGE_SIZE == 0) ?
                             (fileSize / PAGE_SIZE) :
                             (fileSize / PAGE_SIZE) + 1;

    fclose(filePtr);
    return RC_OK;
}

/*
 * Closes an open page file
 * @param fHandle - Pointer to file handle to close
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist,
 *         RC_FILE_HANDLE_NOT_INIT if handle is invalid
 */
extern RC closePageFile(SM_FileHandle *fHandle)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    FILE *filePtr = fopen(fHandle->fileName, "r");
    if (filePtr == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }

    fclose(filePtr);
    return RC_OK;
}

/*
 * Destroys (deletes) a page file
 * @param fileName - Name of the file to destroy
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist
 */
extern RC destroyPageFile(const char *fileName)
{
    FILE *filePtr = fopen(fileName, "r");
    if (filePtr == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }

    fclose(filePtr);

    if (remove(fileName) == 0)
    {
        return RC_OK;
    }
    else
    {
        return RC_FILE_NOT_FOUND;
    }
}

/*
 * Reads a specific block (page) from the file into memory
 * @param pageNum - Page number to read (0-indexed)
 * @param fHandle - Pointer to file handle
 * @param memPage - Buffer to store the read page data (must be at least PAGE_SIZE bytes)
 * @return RC_OK on success, RC_FILE_HANDLE_NOT_INIT if handle is invalid,
 *         RC_READ_NON_EXISTING_PAGE if page doesn't exist,
 *         RC_FILE_NOT_FOUND if file can't be opened
 */
extern RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    if (memPage == NULL)
    {
        return RC_WRITE_FAILED;
    }

    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
    {
        return RC_READ_NON_EXISTING_PAGE;
    }

    FILE *filePtr = fopen(fHandle->fileName, "r");
    if (filePtr == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }

    /* Seek to the requested page */
    if (fseek(filePtr, PAGE_SIZE * pageNum, SEEK_SET) != 0)
    {
        fclose(filePtr);
        return RC_READ_NON_EXISTING_PAGE;
    }

    /* Read the page into memory */
    size_t bytesRead = fread(memPage, sizeof(char), PAGE_SIZE, filePtr);

    /* Update current page position */
    fHandle->curPagePos = pageNum;

    fclose(filePtr);

    if (bytesRead < PAGE_SIZE)
    {
        return RC_READ_NON_EXISTING_PAGE;
    }

    return RC_OK;
}

/*
 * Returns the current page position in the file
 * @param fHandle - Pointer to file handle
 * @return Current page position, or RC_FILE_HANDLE_NOT_INIT if handle is invalid
 */
extern int getBlockPos(SM_FileHandle *fHandle)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return fHandle->curPagePos;
}

/*
 * Reads the first block (page 0) of the file
 * @param fHandle - Pointer to file handle
 * @param memPage - Buffer to store the read page data
 * @return RC_OK on success, error code otherwise
 */
extern RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    return readBlock(0, fHandle, memPage);
}

/*
 * Reads the previous block relative to the current page position
 * @param fHandle - Pointer to file handle
 * @param memPage - Buffer to store the read page data
 * @return RC_OK on success, error code otherwise
 */
extern RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    int prevPageNum = fHandle->curPagePos - 1;
    return readBlock(prevPageNum, fHandle, memPage);
}

/*
 * Reads the current block at the current page position
 * @param fHandle - Pointer to file handle
 * @param memPage - Buffer to store the read page data
 * @return RC_OK on success, error code otherwise
 */
extern RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return readBlock(fHandle->curPagePos, fHandle, memPage);
}

/*
 * Reads the next block relative to the current page position
 * @param fHandle - Pointer to file handle
 * @param memPage - Buffer to store the read page data
 * @return RC_OK on success, error code otherwise
 */
extern RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    int nextPageNum = fHandle->curPagePos + 1;
    return readBlock(nextPageNum, fHandle, memPage);
}

/*
 * Reads the last block in the file
 * @param fHandle - Pointer to file handle
 * @param memPage - Buffer to store the read page data
 * @return RC_OK on success, error code otherwise
 */
extern RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    int lastPageNum = fHandle->totalNumPages - 1;
    return readBlock(lastPageNum, fHandle, memPage);
}

/*
 * Writes a block to a specific page in the file
 * @param pageNum - Page number to write (0-indexed)
 * @param fHandle - Pointer to file handle
 * @param memPage - Buffer containing the data to write (must be PAGE_SIZE bytes)
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file can't be opened,
 *         RC_WRITE_FAILED if write operation fails,
 *         RC_FILE_HANDLE_NOT_INIT if handle is invalid
 */
extern RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    if (memPage == NULL)
    {
        return RC_WRITE_FAILED;
    }

    if (pageNum < 0 || pageNum > fHandle->totalNumPages)
    {
        return RC_WRITE_FAILED;
    }

    /* Open file in read-write mode (don't truncate) */
    FILE *filePtr = fopen(fHandle->fileName, "r+");
    if (filePtr == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }

    /* Seek to the target page */
    if (fseek(filePtr, pageNum * PAGE_SIZE, SEEK_SET) != 0)
    {
        fclose(filePtr);
        return RC_WRITE_FAILED;
    }

    /* Write the page contents */
    size_t bytesWritten = fwrite(memPage, sizeof(char), PAGE_SIZE, filePtr);
    if (bytesWritten != PAGE_SIZE)
    {
        fclose(filePtr);
        return RC_WRITE_FAILED;
    }

    /* Update current page position */
    fHandle->curPagePos = pageNum;

    /* Recalculate total pages in case file grew */
    if (fseek(filePtr, 0L, SEEK_END) != 0)
    {
        fclose(filePtr);
        return RC_READ_NON_EXISTING_PAGE;
    }

    long fileSize = ftell(filePtr);
    if (fileSize == -1)
    {
        fclose(filePtr);
        return RC_READ_NON_EXISTING_PAGE;
    }

    fHandle->totalNumPages = (fileSize % PAGE_SIZE == 0) ?
                             (fileSize / PAGE_SIZE) :
                             (fileSize / PAGE_SIZE) + 1;

    fclose(filePtr);
    return RC_OK;
}

/*
 * Writes a block at the current page position
 * @param fHandle - Pointer to file handle
 * @param memPage - Buffer containing the data to write
 * @return RC_OK on success, error code otherwise
 */
extern RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    return writeBlock(fHandle->curPagePos, fHandle, memPage);
}

/*
 * Appends an empty block (filled with zeros) to the end of the file
 * @param fHandle - Pointer to file handle
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file can't be opened,
 *         RC_WRITE_FAILED if write operation fails
 */
extern RC appendEmptyBlock(SM_FileHandle *fHandle)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    FILE *filePtr = fopen(fHandle->fileName, "a+");
    if (filePtr == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }

    /* Create a new empty block */
    SM_PageHandle newBlock = (SM_PageHandle)calloc(PAGE_SIZE, sizeof(char));
    if (newBlock == NULL)
    {
        fclose(filePtr);
        return RC_WRITE_FAILED;
    }

    /* Seek to end of file */
    if (fseek(filePtr, 0, SEEK_END) != 0)
    {
        free(newBlock);
        fclose(filePtr);
        return RC_WRITE_FAILED;
    }

    /* Write the empty block */
    size_t bytesWritten = fwrite(newBlock, sizeof(char), PAGE_SIZE, filePtr);

    free(newBlock);
    fclose(filePtr);

    if (bytesWritten != PAGE_SIZE)
    {
        return RC_WRITE_FAILED;
    }

    /* Update total page count */
    fHandle->totalNumPages++;

    return RC_OK;
}

/*
 * Ensures the file has at least the specified number of pages
 * If the file has fewer pages, empty pages are appended
 * @param numberOfPages - Minimum number of pages required
 * @param fHandle - Pointer to file handle
 * @return RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist,
 *         error code from appendEmptyBlock if append fails
 */
extern RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle)
{
    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    FILE *filePtr = fopen(fHandle->fileName, "r");
    if (filePtr == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }
    fclose(filePtr);

    /* If capacity is already sufficient, return success */
    if (fHandle->totalNumPages >= numberOfPages)
    {
        return RC_OK;
    }

    /* Append empty blocks until we reach the required capacity */
    while (fHandle->totalNumPages < numberOfPages)
    {
        RC result = appendEmptyBlock(fHandle);
        if (result != RC_OK)
        {
            return result;
        }
    }

    return RC_OK;
}
