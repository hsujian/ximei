#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "fmempool.h"

fmempool_ptr_t new_fmempool(const char *pathname, int flags, mode_t mode, uint32_t elt_size, uint32_t nalloc)
{
	int save_errno = 0;
	int fd = open(pathname, flags, mode);
	if (-1 == fd) {
		return MAP_FAILED;
	}
	off_t filelen = sizeof(fmempool_t) + elt_size * nalloc;
	int ret = ftruncate(fd, filelen);
	if (-1 == ret) {
		save_errno = errno;
		close(fd);
		errno = save_errno;
		return MAP_FAILED;
	}
	fmempool_ptr_t mem = (fmempool_ptr_t) mmap (NULL, filelen, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	save_errno = errno;
	close(fd);
	errno = save_errno;
	if (MAP_FAILED == (void*) mem) {
		return MAP_FAILED;
	}
	mem->elt_size = elt_size;
	mem->nalloc = nalloc;
	mem->nelts = 0;
	mem->free_head = 0;
	mem->free_tail = 0;
	mem->elts[0].len = nalloc;
	mem->elts[0].next = nalloc;
	return mem;
}

fmempool_ptr_t load_fmempool(const char *pathname, int flags, mode_t mode)
{
	int save_errno = 0;
	int fd = open(pathname, O_RDWR);
	if (-1 == fd) {
		return MAP_FAILED;
	}
	struct stat st;
	int ret = fstat(fd, &st);
	if (-1 == ret) {
		save_errno = errno;
		close(fd);
		errno = save_errno;
		return MAP_FAILED;
	}
	if (st.st_size < 1) {
		close(fd);
		errno = EBADF;
		return MAP_FAILED;
	}
	fmempool_ptr_t mem = (fmempool_ptr_t) mmap (NULL, st.st_size, flags, mode, fd, 0);
	save_errno = errno;
	close(fd);
	errno = save_errno;
	return mem;
}

int fmempool_sync(fmempool_ptr_t ptr)
{
	return msync(ptr, sizeof(fmempool_t) + ptr->elt_size * ptr->nalloc, MS_INVALIDATE | MS_SYNC);
}

int fmempool_free(fmempool_ptr_t ptr)
{
	return munmap(ptr, sizeof(fmempool_t) + ptr->elt_size * ptr->nalloc);
}

