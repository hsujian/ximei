#include "mmap_util.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef MAX_PATH
# define MAX_PATH 2048
#endif

static int mmap_file_open(mmap_file_t &mft, const char *filepath, int prot)
{
	mft.size = 0;
	mft.mm = (char *)MAP_FAILED;
	struct stat st;
	int flags = O_LARGEFILE;
	if (prot & PROT_WRITE) {
		flags |= O_RDWR;
	} else {
		flags |= O_RDONLY;
	}
	int fd = open(filepath, flags);
	if (fd < 0) {
		return MMAP_FILE_ERROR_OPEN_FAIL;
	}
	if(fstat(fd, &st) < -1) {
		int eno = errno;
		close(fd);
		errno = eno;
		return MMAP_FILE_ERROR_STAT_FAIL;
	}

	mft.size = st.st_size;
	mft.mm = (char *)mmap(NULL, mft.size, prot, MAP_SHARED, fd, 0);
	if (MAP_FAILED == (void *)mft.mm) {
		int eno = errno;
		close(fd);
		errno = eno;
		mft.size = 0;
		return MMAP_FILE_ERROR_MMAP_FAIL;
	}
	close(fd);
	return MMAP_FILE_ERROR_OK;
}

int mmap_file_readonly_open(mmap_file_t &mft, const char *filepath)
{
	return mmap_file_open(mft, filepath, PROT_READ);
}

void mmap_file_close(mmap_file_t &mft)
{
	if (mft.size > 0 && (void *) mft.mm != MAP_FAILED) {
		munmap((void *)mft.mm, mft.size);
		mft.size = 0;
		mft.mm = (char *)MAP_FAILED;
	}
}

int mmap_file_rw_open(mmap_file_t &mft, const char *filepath)
{
	return mmap_file_open(mft, filepath, PROT_READ|PROT_WRITE);
}

int creat_or_truncate(const char *filepath, off_t size)
{
	struct stat st;
	int rv = stat (filepath, &st);
	if  (rv == -1) {
		if (errno == ENOENT) {
			rv = creat (filepath, 0644);
			if (rv == -1) {
				return MMAP_FILE_ERROR_OPEN_FAIL;
			}
			st.st_size = 0;
		} else {
			return MMAP_FILE_ERROR_STAT_FAIL;
		}
	}

	if (st.st_size < size) {
		rv = truncate (filepath, size);
		if (rv == -1) {
			return MMAP_FILE_ERROR_TRUNC_FAIL;
		}
	}

	return 0;
}

void mmap_file_array_close(mmap_file_array_t *ptr)
{
	if (ptr == NULL) {
		return;
	}
	munmap((void *)ptr, ptr->mmap_size);
}

static mmap_file_array_t * mmap_file_array_open(const char *filepath, int prot)
{
	mmap_file_t mft;
	int rv = mmap_file_open(mft, filepath, prot);
	if (rv != MMAP_FILE_ERROR_OK) {
		return NULL;
	}
	return (mmap_file_array_t *) mft.mm;
}

mmap_file_array_t * mmap_file_array_readonly_open(const char *filepath)
{
	return mmap_file_array_open(filepath, PROT_READ);
}

mmap_file_array_t * mmap_file_array_rw_open(const char *filepath)
{
	return mmap_file_array_open(filepath, PROT_READ | PROT_WRITE);
}

mmap_file_array_t *mmap_file_array_creat(const char *filepath, int item_size, int item_max_num)
{
	off_t size = sizeof(mmap_file_array_t) + item_size * item_max_num;
	int rv = creat_or_truncate(filepath, size);
	if (rv != MMAP_FILE_ERROR_OK) {
		return NULL;
	}
	mmap_file_t mft;
	rv = mmap_file_open(mft, filepath, PROT_READ | PROT_WRITE);
	if (rv != MMAP_FILE_ERROR_OK) {
		return NULL;
	}
	mmap_file_array_t *ptr = (mmap_file_array_t *)mft.mm;
	ptr->padding_1 = 0;
	ptr->max_num = item_max_num;
	ptr->cur_num = 0;
	ptr->mmap_size = size;
	ptr->item_size = item_size;
	ptr->padding_2 = 0;
	return ptr;
}

