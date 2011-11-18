#ifndef __MMAP_MEMPOOL_H_
#define __MMAP_MEMPOOL_H_

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/limits.h>

typedef struct {
	int32_t block_idx;
	uint64_t usable_size;
	uint64_t offset;
} mem_alloc_info_t;

typedef struct {
	int32_t ref;
	uint64_t last;
	uint64_t recycle;
	char mem[0];
} mmap_block_t;

typedef struct {
	uint32_t block_max_num;
	uint64_t block_size;
	mmap_block_t **block;
	char tag[PATH_MAX+1];
} mmap_mempool_t;

mmap_mempool_t *
mmap_mempool_create(const uint64_t block_size, const uint32_t block_max_num, const char *tag);

int 
mmap_mempool_load(mmap_mempool_t *pool);

void
mmap_mempool_sync(mmap_mempool_t *pool, int flags);

void
mmap_mempool_close(mmap_mempool_t *pool);

int
mmap_mempool_free(mmap_mempool_t *pool, const char *ptr);

int
mmap_mempool_ref(mmap_mempool_t *pool, const char *ptr);

int
mmap_mempool_unref(mmap_mempool_t *pool, const char *ptr);

uint64_t
mmap_mempool_usable_size(mmap_mempool_t *pool, const char *ptr);

void *
mmap_mempool_alloc(mmap_mempool_t *pool, const size_t size);

void *
mmap_mempool_calloc(mmap_mempool_t *pool, const size_t size);

int
mmap_mempool_alloc_info(mmap_mempool_t *pool, const char *ptr, mem_alloc_info_t *mem_alloc_info);

#endif

