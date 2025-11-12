CC = gcc

# compiler flags:
#  -g           adds debugging information to the executable file
#  -Wall        turns on most compiler warnings
#  -Wextra      enables additional warnings not covered by -Wall
#  -Wpedantic   issues warnings for code that doesn't follow strict ISO C
#  -Werror      treats all warnings as errors (disabled by default)
#  -std=c99     uses C99 standard
#  -O2          optimization level 2 for production (use -O0 for debugging)
CFLAGS = -g -Wall -Wextra -Wpedantic -std=c99 -O2

all : prog

prog : test_assign1_1.o storage_mgr.o dberror.o
	$(CC) $(CFLAGS) -o assign_str_mgr test_assign1_1.o storage_mgr.o dberror.o 

storage_mgr.o : storage_mgr.c storage_mgr.h 
	$(CC) $(CFLAGS) -c storage_mgr.c 

dberror.o : dberror.c dberror.h
	$(CC) $(CFLAGS) -c dberror.c 

test_assign1_1.o : test_assign1_1.c
	$(CC) $(CFLAGS) -c test_assign1_1.c

clean: 
	$(RM) prog *.o *~ assign_str_mgr

run:
	./assign_str_mgr
