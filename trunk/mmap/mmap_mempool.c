#include "mmap_mempool.h"

#ifndef MAX_PATH
# define MAX_PATH 2048
#endif

#define MFM_ALIGNMENT   sizeof(unsigned long)    /* platform word */

#define mfm_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define mfm_align_ptr(p, a)                                                   \
    (char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

typedef struct {
	int32_t ref;
	uint64_t usable_size;
	char mem[0];
} mem_control_t;

typedef struct {
	uint64_t next;
	uint64_t usable_size;
} mem_recycle_t;

static int mkdirp(const char *dir, mode_t mode)
{
	int rv = mkdir (dir, mode);
	if (!(rv == -1 && errno == ENOTDIR)) {
		return rv;
	}
	char tmp[PATH_MAX+1];
	int len = strlen(dir);
	if (len < 1) {
		errno = EFAULT;
		return -1;
	}
	if (len > PATH_MAX) {
		errno = ENAMETOOLONG;
		return -1;
	}

	strncpy(tmp, dir, len);
	tmp[len] = '\0';
	if (tmp[len - 1] == '/') {
		tmp[len - 1] = '\0';
		len--;
		if (len < 1) {
			errno = EFAULT;
			return -1;
		}
	}

	int i = len;
	for (i=len; i>0; i--) {
		if (tmp[i] == '/') {
			tmp[i] = '\0';
			rv = mkdir (tmp, mode);
			tmp[i] = '/';
			if (rv == -1 && errno == ENOTDIR) {
				continue;
			}
			if (rv == 0) {
				break;
			}
		}
	}
	for (; i<len; i++) {
		if (tmp[i] == '/') {
			tmp[i] = '\0';
			rv = mkdir (tmp, mode);
			tmp[i] = '/';
			if (rv != 0) {
				return rv;
			}
		}
	}
	return mkdir (tmp, mode);
}

mmap_mempool_t *
mmap_mempool_create(const uint64_t block_size, const uint32_t block_max_num, const char *tag)
{
	mmap_mempool_t *pool = (mmap_mempool_t *)calloc(1, sizeof(mmap_mempool_t));
	if (pool == NULL) {
		return NULL;
	}
	pool->block_size = block_size + sizeof(mem_control_t);
	pool->block_max_num = block_max_num;
	if (tag != NULL) {
		memcpy(pool->tag, tag, PATH_MAX);
		pool->tag[PATH_MAX] = '\0';

		char path[PATH_MAX+1];
		memcpy(path, tag, PATH_MAX);
		path[PATH_MAX] = '\0';
		char *str=strrchr(path, '/');
		if (str!=NULL && str > path) {
			*str = '\0';
			mkdirp(path, 0644);
		}
	} else {
		pool->tag[0] = '\0';
	}

	pool->block = (mmap_block_t **)malloc(block_max_num * sizeof(mmap_block_t*));
	if (pool->block == NULL) {
		free(pool);
		return NULL;
	}
	int i;
	for (i=pool->block_max_num; i--; ) {
		pool->block[i] = (mmap_block_t *)MAP_FAILED;
	}
	return pool;
}

int 
mmap_mempool_load(mmap_mempool_t *pool)
{
	if (pool->tag[0] == '\0') {
		return 0;
	}
	int load_num = 0;
	int i;
	char tmp[MAX_PATH+1];
	for (i=pool->block_max_num; i--;) {
		pool->block[i] = (mmap_block_t *)MAP_FAILED;
		snprintf(tmp, sizeof(tmp), "%s.%d", pool->tag, i);
		int fd = open(tmp, O_RDWR | O_LARGEFILE);
		if (fd < 0) {
			continue;
		}
		struct stat st;
		if(fstat(fd, &st) < -1) {
			int err = errno;
			close(fd);
			errno = err;
			return -1;
		}

		if (st.st_size < pool->block_size) {
			if (ftruncate(fd, pool->block_size) == -1) {
				int err = errno;
				close(fd);
				errno = err;
				return -1;
			}
		}
		pool->block[i] = (mmap_block_t *)mmap(NULL, pool->block_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
		if (MAP_FAILED == (void *)pool->block[i]) {
			int err = errno;
			close(fd);
			errno = err;
			return -1;
		}
		close(fd);
		load_num++;
	}
	return load_num;
}

void
mmap_mempool_sync(mmap_mempool_t *pool, int flags)
{
	if (pool->tag[0] == '\0') {
		return;
	}
	int i;
	if (flags == 0) {
		flags = MS_ASYNC;
	}
	for (i=pool->block_max_num; i--;) {
		if (MAP_FAILED != (void *)pool->block[i]) {
			msync((void *)pool->block[i], pool->block_size, flags);
		}
	}
}

static int 
mmap_mempool_open_block(mmap_mempool_t *pool, const int idx)
{
	if ((void*)pool->block[idx] != MAP_FAILED) {
		return -1;
	}

	if (pool->tag[0] == '\0') {
		char tmp[MAX_PATH+1];
		snprintf(tmp, sizeof(tmp), "%s.%d", pool->tag, idx);
		int fd = open(tmp, O_RDWR | O_LARGEFILE | O_CREAT | O_TRUNC, 0644);
		if (fd < 0) {
			return -1;
		}

		if (ftruncate(fd, pool->block_size) == -1) {
			int err = errno;
			close(fd);
			errno = err;
			return -1;
		}

		pool->block[idx] = (mmap_block_t *)mmap(NULL, pool->block_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
		if (MAP_FAILED == (void *)pool->block[i]) {
			int err = errno;
			close(fd);
			errno = err;
			return -1;
		}
		close(fd);
	} else {
		pool->block[idx] = (mmap_block_t *)mmap(NULL, pool->block_size, PROT_WRITE | PROT_READ, MAP_ANONYMOUS, -1, 0);
		if (MAP_FAILED == (void *)pool->block[i]) {
			return -1;
		}
	}

	pool->block[idx]->ref = 0;
	pool->block[idx]->last = 0;
	pool->block[idx]->recycle = (uint64_t) -1;

	return 0;
}

static int 
mmap_mempool_close_block(mmap_mempool_t *pool, const int idx)
{
	if ((void*)pool->block[idx] != MAP_FAILED) {
		void *m = pool->block[idx];
		pool->block[idx] = (char *)MAP_FAILED;
		return munmap(m, pool->block_size);
	}
	return 0;
}

void *
mmap_mempool_alloc(mmap_mempool_t *pool, const size_t size)
{
	size_t need_size = size + sizeof(mem_control_t);
	need_size = mfm_align(need_size, MFM_ALIGNMENT);
	size_t usable_size = need_size - offsetof(mem_control_t, mem);
	if (need_size <= pool->block_size) {
		int i;
		for (i=0; i<pool->block_max_num; i++) {
			if (MAP_FAILED == (void *)pool->block[i]) {
				int rv = mmap_mempool_open_block(pool, i);
				if (rv == -1) {
					return NULL;
				}
			}
			if ((size_t)(pool->block_size - pool->block[i]->last) >= need_size) {
				++ pool->block[i]->ref;
				mem_control_t *mc = (mem_control_t *) (pool->block[i]->mem + pool->block[i]->last);
				mc->ref = 1;
				mc->usable_size = usable_size;
				pool->block[i]->last += need_size;
				return (void *)mc->mem;
			}
		}
	}

	return NULL;
}

int
mmap_mempool_free(mmap_mempool_t *pool, const char *ptr)
{
	int i;
	for (i=0; i<pool->block_max_num; i++) {
		if (MAP_FAILED != (void *)pool->block[i]) {
			if (ptr > pool->block[i]->mem && ptr - pool->block[i]->mem < pool->block_size) {
				mem_control_t *mc = (mem_control_t *)(ptr - offsetof(mem_control_t, mem));
				uint64_t usable_size = mc->usable_size;
				mem_recycle_t *mr = (mem_recycle_t *)mc;
				mr->next = pool->block[i]->recycle;
				mr->usable_size = usable_size;
				pool->block[i]->recycle = mr->next; 
				return 0;
			}
		}
	}
	return -1;
}

int
mmap_mempool_ref(mmap_mempool_t *pool, const char *ptr)
{
	int i;
	for (i=0; i<pool->block_max_num; i++) {
		if (MAP_FAILED != (void *)pool->block[i]) {
			if (ptr > pool->block[i]->mem && ptr - pool->block[i]->mem < pool->block_size) {
				mem_control_t *mc = (mem_control_t *)(ptr - offsetof(mem_control_t, mem));
				++ mc->ref;
				return 0;
			}
		}
	}
	return -1;
}

int
mmap_mempool_unref(mmap_mempool_t *pool, const char *ptr)
{
	int i;
	for (i=0; i<pool->block_max_num; i++) {
		if (MAP_FAILED != (void *)pool->block[i]) {
			if (ptr > pool->block[i]->mem && ptr - pool->block[i]->mem < pool->block_size) {
				mem_control_t *mc = (mem_control_t *)(ptr - offsetof(mem_control_t, mem));
				-- mc->ref;
				if (mc->ref < 1) {
					mmap_mempool_free(pool, ptr);
				}
				return 0;
			}
		}
	}
	return -1;
}

uint64_t
mmap_mempool_usable_size(mmap_mempool_t *pool, const char *ptr)
{
	int i;
	for (i=0; i<pool->block_max_num; i++) {
		if (MAP_FAILED != (void *)pool->block[i]) {
			if (ptr > pool->block[i]->mem && ptr - pool->block[i]->mem < pool->block_size) {
				mem_control_t *mc = (mem_control_t *)(ptr - offsetof(mem_control_t, mem));
				return mc->usable_size;
			}
		}
	}
	return malloc_usable_size(ptr);
}

void *
mmap_mem(mem_control_t *)(ptr - offsetof(mem_control_t, mem));
				return mc->usable_size;
			}
		}
	}
	return malloc_usable_size(ptr);
}

void *
mmap_mempool_calloc(mmap_mempool_t *pool, const size_t size)
{
    void *p;

    p = mmap_mempool_alloc(pool, size);
    if (p) {
        memset(p, 0, size);
    }

    return p;
}

int
mmap_mempool_alloc_info(mmap_mempool_t *pool, const char *ptr, mem_alloc_info_t *mem_alloc_info)
{
	mem_alloc_info->block_idx = -1;
	mem_alloc_info->usable_size = 0;
	mem_alloc_info->offset = 0;
	int i;
	for (i=0; i<pool->block_max_num; i++) {
		if (MAP_FAILED != (void *)pool->block[i]) {
			if (ptr > pool->block[i]->mem && ptr - pool->block[i]->mem < pool->block_size) {
				mem_control_t *mc = (mem_control_t *)(ptr - offsetof(mem_control_t, mem));
				mem_alloc_info->block_idx = i;
				mem_alloc_info->usable_size = mc->usable_size;
				mem_alloc_info->offset = ptr - pool->block[i]->mem;
				return 0;
			}
		}
	}
	return -1;
}

