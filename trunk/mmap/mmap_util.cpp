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

void mmap_file_array_close(mmap_file_array_t &mft)
{
	mmap_file_close(mft.data);
	mmap_file_close(mft.item_info);
}

static int mmap_file_array_open(mmap_file_array_t &mft, const char *filepath, int prot)
{
	int rv = mmap_file_open(mft.data, filepath, prot);
	if (rv != MMAP_FILE_ERROR_OK) {
		return rv;
	}
	char info_file[MAX_PATH];
	snprintf(info_file, sizeof(info_file), "%s.binfo", filepath);
	rv = mmap_file_open(mft.item_info, info_file, prot);
	if (rv != MMAP_FILE_ERROR_OK) {
		int eno = errno;
		mmap_file_array_close(mft);
		errno = eno;
		return rv;
	}
	int *item_info = (int *)mft.item_info.mm;
	mft.item_cur_num = &item_info[0];
	mft.item_max_num = &item_info[1];
	mft.item_size = &item_info[2];
	return MMAP_FILE_ERROR_OK;
}

int mmap_file_array_readonly_open(mmap_file_array_t &mft, const char *filepath)
{
	return mmap_file_array_open(mft, filepath, PROT_READ);
}

int mmap_file_array_rw_open(mmap_file_array_t &mft, const char *filepath)
{
	return mmap_file_array_open(mft, filepath, PROT_READ | PROT_WRITE);
}

#define MAX_BINFO_INT_NUM 256

int mmap_file_array_creat(const char *filepath, off_t size, int item_size, int item_max_num)
{
	char info_file[MAX_PATH];
	snprintf(info_file, sizeof(info_file), "%s.binfo", filepath);
	int rv = creat_or_truncate(info_file, sizeof(int) * MAX_BINFO_INT_NUM);
	if (rv != MMAP_FILE_ERROR_OK) {
		return rv;
	}

	rv = creat_or_truncate(filepath, size);
	if (rv != MMAP_FILE_ERROR_OK) {
		return rv;
	}
	mmap_file_t binfo_mft;
	rv = mmap_file_open(binfo_mft, info_file, PROT_READ | PROT_WRITE);
	if (rv != MMAP_FILE_ERROR_OK) {
		return rv;
	}
	int *binfo = (int *)binfo_mft.mm;
	binfo[0] = 0;
	binfo[1] = item_max_num;
	binfo[2] = item_size;
	mmap_file_close(binfo_mft);
	return 0;
}

