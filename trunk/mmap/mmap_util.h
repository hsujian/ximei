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
	int *item_cur_num;
	int *item_max_num;
	int *item_size;
	mmap_file_t data;
	mmap_file_t item_info;
}mmap_file_array_t;

extern int mmap_file_array_readonly_open(mmap_file_array_t &mft, const char *filepath);

extern int mmap_file_array_rw_open(mmap_file_array_t &mft, const char *filepath);

extern int mmap_file_array_rw_creat(const char *filepath, off_t size, int item_size, int item_max_num);

extern void mmap_file_array_close(mmap_file_array_t &mft);

#define get_mfarray_item_cur_num(mmap_file_array_obj) (*((mmap_file_array_obj).item_cur_num))
#define get_mfarray_item_max_num(mmap_file_array_obj) (*((mmap_file_array_obj).item_max_num))
#define get_mfarray_item_size(mmap_file_array_obj) (*((mmap_file_array_obj).item_size))

#define set_mfarray_item_cur_num(mmap_file_array_obj, cur_num) *((mmap_file_array_obj).item_cur_num) = (cur_num)

#endif
