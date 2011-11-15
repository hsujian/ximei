#ifndef __MMAP_UTIL_H_
#define __MMAP_UTIL_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define MMAP_FILE_ERROR_OK 0
#define MMAP_FILE_ERROR_OPEN_FAIL -1
#define MMAP_FILE_ERROR_STAT_FAIL -2
#define MMAP_FILE_ERROR_MMAP_FAIL -3
#define MMAP_FILE_ERROR_TRUNC_FAIL -4

typedef struct {
	char *mm;
	off_t size;
}mmap_file_t;

extern int mmap_file_readonly_open(mmap_file_t &mft, const char *filepath);

extern int mmap_file_rw_open(mmap_file_t &mft, const char *filepath);

extern int creat_or_truncate(const char *filepath, off_t size);

extern void mmap_file_close(mmap_file_t &mft);


typedef struct {
	int padding_1;
	int version;
	int max_num;
	int cur_num;
	off_t mmap_size;
	int item_size;
	int padding_2;
	char data[0];
}mmap_file_array_t;

extern mmap_file_array_t * mmap_file_array_readonly_open(const char *filepath);

extern mmap_file_array_t * mmap_file_array_rw_open(const char *filepath);

extern mmap_file_array_t *mmap_file_array_creat(const char *filepath, int item_size, int item_max_num);

extern void mmap_file_array_close(mmap_file_array_t *ptr);

#endif
