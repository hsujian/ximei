#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "farray.h"

farray_ptr_t new_farray(const char *pathname, int flags, mode_t mode, int block_size, int block_num)
{
	int fd = open(pathname, flags, mode);
	if (-1 == fd) {
		return MAP_FAILED;
	}
	off_t filelen = sizeof(farray_t) + block_size * block_num;
	int ret = ftruncate(fd, filelen);
	if (-1 == ret) {
		close(fd);
		return MAP_FAILED;
	}
	farray_ptr_t mem = (farray_ptr_t) mmap (NULL, filelen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (MAP_FAILED == (void*) mem) {
		return MAP_FAILED;
	}
	mem->elt_size = block_size;
	mem->nalloc = block_num;
	mem->nelts = 0;
	return mem;
}

farray_ptr_t load_farray(const char *pathname, int flags, mode_t mode)
{
	int fd_flags = 0;
	if (flags == PROT_READ) {
		fd_flags = O_RDONLY;
	} else {
		fd_flags = O_RDWR;
	}
	int fd = open(pathname, fd_flags);
	if (-1 == fd) {
		return MAP_FAILED;
	}
	struct stat st;
	int ret = fstat(fd, &st);
	if (-1 == ret) {
		close(fd);
		return MAP_FAILED;
	}
	if (st.st_size < 1) {
		close(fd);
		return MAP_FAILED;
	}
	farray_ptr_t mem = (farray_ptr_t) mmap (NULL, st.st_size, flags, mode, fd, 0);
	close(fd);
	return mem;
}

int farray_sync(farray_ptr_t ptr)
{
	return msync(ptr, sizeof(farray_t) + ptr->elt_size * ptr->nalloc, MS_INVALIDATE | MS_SYNC);
}

int farray_free(farray_ptr_t ptr)
{
	return munmap(ptr, sizeof(farray_t) + ptr->elt_size * ptr->nalloc);
}

