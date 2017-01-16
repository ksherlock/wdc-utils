#include <stdio.h>
#include <fcntl.h>


void main(void) {
	
	FILE *fp = fopen("file.txt", "wb");

	fclose(fp);
}

// these stubs are not provided in the library but could call gs/os.

int open(const char *name, int mode) {
	return -1;
}

int close(int fd) {
	return -1;
}

size_t read(int fd, void *buffer, size_t count) {
	return -1;
}

size_t write(int fd, void *buffer, size_t count) {
	return -1;
}

long lseek(int fd, long offset, int whence) {
	return -1;
}

int creat(const char *name, int mode) {
	return -1;
}

int unlink(const char *name) {
	return -1;
}

int isatty(int fd) {
	return -1;
}


#pragma section udata=heap
char __heap[8092];
void *heap_start = (void *)__heap;
void *heap_end = (void *)&__heap[8092];
