================================================================================
                    CS525 - Assignment 1: Storage Manager
================================================================================

================================================================================
                              PROJECT OVERVIEW
================================================================================

This assignment implements a Storage Manager - a foundational module for a
database management system that provides an interface for reading and writing
blocks (pages) to files on disk. The storage manager abstracts the OS-level
file operations and provides a simple page-based API for higher-level database
components.

The implementation has been enhanced to meet production-quality standards with
comprehensive error handling, memory safety, and clean code practices.

================================================================================
                           HOW TO BUILD AND RUN
================================================================================

Build the Project:
------------------
    make

This compiles all source files with strict compiler flags (-Wall -Wextra
-Wpedantic -std=c99) to ensure code quality and standards compliance.

Run the Tests:
--------------
    ./assign_str_mgr

Or use the make target:
    make run

Clean Build Artifacts:
----------------------
    make clean

This removes all object files, executables, and temporary files.

================================================================================
                              FILE STRUCTURE
================================================================================

Source Files:
-------------
    storage_mgr.c       - Main implementation of the storage manager
    storage_mgr.h       - Public interface and data structures
    dberror.c          - Error handling implementation
    dberror.h          - Error codes and error handling macros
    test_assign1_1.c   - Test suite for storage manager functions
    test_helper.h      - Testing utilities and macros

Build Files:
------------
    Makefile           - Build configuration with production-quality flags
    README.txt         - This file

================================================================================
                          STORAGE MANAGER API
================================================================================

The Storage Manager provides the following categories of operations:

1. FILE MANAGEMENT OPERATIONS
------------------------------

initStorageManager()
    Initializes the storage manager (currently no initialization needed)

createPageFile(fileName)
    Creates a new page file with one empty page (4096 bytes)
    Returns: RC_OK on success, RC_FILE_NOT_FOUND on failure

openPageFile(fileName, fHandle)
    Opens an existing page file and populates the file handle with metadata
    Returns: RC_OK on success, error code on failure

closePageFile(fHandle)
    Closes an open page file
    Returns: RC_OK on success, error code on failure

destroyPageFile(fileName)
    Deletes a page file from disk
    Returns: RC_OK on success, RC_FILE_NOT_FOUND if file doesn't exist


2. READ OPERATIONS
------------------

readBlock(pageNum, fHandle, memPage)
    Reads a specific page number into memory
    Returns: RC_OK on success, error code on failure

readFirstBlock(fHandle, memPage)
    Reads the first page (page 0) of the file

readPreviousBlock(fHandle, memPage)
    Reads the page before the current page position

readCurrentBlock(fHandle, memPage)
    Reads the page at the current page position

readNextBlock(fHandle, memPage)
    Reads the page after the current page position

readLastBlock(fHandle, memPage)
    Reads the last page in the file

getBlockPos(fHandle)
    Returns the current page position (0-indexed)


3. WRITE OPERATIONS
-------------------

writeBlock(pageNum, fHandle, memPage)
    Writes a page to a specific position in the file
    Returns: RC_OK on success, error code on failure

writeCurrentBlock(fHandle, memPage)
    Writes a page at the current page position

appendEmptyBlock(fHandle)
    Appends a new empty page to the end of the file

ensureCapacity(numberOfPages, fHandle)
    Ensures the file has at least the specified number of pages
    Appends empty pages if necessary

================================================================================
                      CODE IMPROVEMENTS
================================================================================

The code has been significantly enhanced from the initial implementation. 
The following improvements were made:

CODE QUALITY IMPROVEMENTS:
--------------------------
✓ Removed all dead code
✓ Eliminated excessive debug output for clean, professional operation
✓ Improved variable naming conventions (TempFile → filePtr, etc.)
✓ Added comprehensive function documentation with parameter descriptions
✓ Consistent code formatting and style throughout
✓ Added const qualifiers where appropriate for better API design

BUG FIXES:
----------
✓ CRITICAL: Fixed writeBlock file mode from 'w' to 'r+'
    - Previous code truncated entire file on write operations
    - Now correctly updates files without data loss

✓ Fixed curPagePos calculation in readBlock
    - Was using byte offsets instead of page numbers
    - Now correctly tracks page position

✓ Fixed appendEmptyBlock validation
    - Corrected fwrite return value check from != 1 to != PAGE_SIZE
    - Ensures all bytes are written successfully

✓ Fixed hardcoded error return values
    - Changed -1 to proper RC_READ_NON_EXISTING_PAGE constant
    - Consistent error code usage throughout

SAFETY & ROBUSTNESS:
--------------------
✓ Added NULL pointer checks for all function parameters
✓ Added memory allocation failure checks after calloc/malloc
✓ Improved error handling with proper resource cleanup on all error paths
✓ All file handles properly closed before returning errors
✓ All allocated memory freed on failure paths
✓ Input validation for page numbers and buffer pointers

BUILD & COMPILATION:
--------------------
✓ Enhanced Makefile with strict compiler flags:
    -Wall -Wextra -Wpedantic -std=c99 -O2
✓ Code compiles with ZERO warnings
✓ Optimized for production with -O2 flag
✓ Strict C99 standards compliance
✓ Fixed all compiler warnings (missing newlines, etc.)

TESTING & VERIFICATION:
-----------------------
✓ All existing tests pass successfully
✓ No memory leaks (all allocations properly freed)
✓ Robust error handling verified
✓ Edge cases properly handled

================================================================================
                           IMPLEMENTATION DETAILS
================================================================================

Page Size:
----------
The storage manager uses a fixed page size of 4096 bytes (PAGE_SIZE constant
defined in dberror.h). This is a common page size used in many database
systems and matches typical OS page sizes.

File Handle Structure:
----------------------
typedef struct SM_FileHandle {
    char *fileName;        // Name of the page file
    int totalNumPages;     // Total number of pages in the file
    int curPagePos;        // Current page position (0-indexed)
    void *mgmtInfo;        // Additional management info (unused)
} SM_FileHandle;

Error Codes:
------------
RC_OK                        (0) - Operation successful
RC_FILE_NOT_FOUND           (1) - File doesn't exist or can't be created
RC_FILE_HANDLE_NOT_INIT     (2) - File handle not initialized
RC_WRITE_FAILED             (3) - Write operation failed
RC_READ_NON_EXISTING_PAGE   (4) - Attempted to read non-existent page

Memory Management:
------------------
- All memory allocations use calloc for zero-initialization
- Comprehensive NULL checks after all allocations
- Proper cleanup on all error paths
- No memory leaks in normal or error scenarios

File Operations:
----------------
- Files opened in appropriate modes (r, r+, w+, a+)
- All file handles properly closed
- File operations checked for errors with proper return codes
- Atomicity considerations for write operations

================================================================================
                          DESIGN DECISIONS
================================================================================

1. File Mode Selection:
   - createPageFile uses "w+" (create/truncate, read-write)
   - openPageFile uses "r" (read-only for metadata)
   - writeBlock uses "r+" (read-write, no truncation)
   - appendEmptyBlock uses "a+" (append mode)

2. Error Handling Strategy:
   - Fail fast with appropriate error codes
   - Clean up resources before returning errors
   - Consistent error code usage throughout

3. API Design:
   - Simple, intuitive function names
   - Const correctness for read-only parameters
   - Comprehensive documentation for all public functions
   - Clear separation of concerns

4. Performance Considerations:
   - O2 optimization enabled for production
   - Minimal file operations per function call
   - Efficient page size calculations
   - Zero-copy operations where possible

================================================================================
                           TESTING STRATEGY
================================================================================

The test suite (test_assign1_1.c) validates:

1. File Creation and Destruction:
   - Creating new page files
   - Opening existing files
   - Closing file handles
   - Destroying files
   - Error handling for non-existent files

2. Page Content Operations:
   - Reading empty pages
   - Writing data to pages
   - Reading back written data
   - Data integrity verification

3. Edge Cases:
   - Operations on non-existent files
   - Invalid page numbers
   - Boundary conditions

All tests pass successfully.

================================================================================
                              NOTES
================================================================================

- This implementation is thread-safe for read operations on different files
- Write operations should be externally synchronized if concurrent access needed
- File handles should be properly closed to avoid resource leaks
- The implementation assumes a Unix-like file system
- Page numbers are 0-indexed throughout the API

================================================================================
                         ACADEMIC INTEGRITY
================================================================================

This code represents original work by Zoraiz Sibtain for CS525. All improvements
and enhancements maintain the core functionality while improving code functionality. 
The code is suitable for academic submission and professional use.

================================================================================
                           VERSION HISTORY
================================================================================

Version 1.0 (Initial):
- Basic implementation of storage manager functionality
- Core read/write operations
- Basic error handling

Version 2.0:
- Fixed critical bugs in file operations
- Added comprehensive error handling and safety checks
- Removed all dead code and debug output
- Added full documentation
- Enhanced build system with strict compiler flags
- Achieved zero-warning compilation
- All tests passing

================================================================================
                            END OF README
================================================================================