#ifndef __MMAP_UTIL_H_
#define __MMAP_UTIL_H_


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>


typedef struct {
	char *mm;
	off_t size;
}mmap_file_t;

extern int mmap_file_readonly_open(mmap_file_t &mft, const char *filepath);

extern void mmap_file_close(mmap_file_t &mft);

#endif
