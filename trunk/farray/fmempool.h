#ifndef _FILE_MEMPOOL_H_INCLUDE_
#define _FILE_MEMPOOL_H_INCLUDE_

#include <stdint.h>

typedef struct {
	uint32_t len;
	uint32_t next;
} free_info_t;

typedef struct{
	/** The amount of memory allocated for each element of the array */
	uint32_t elt_size;
	/** The number of active elements in the array */
	uint32_t nelts;
	/** The number of elements allocated in the array */
	uint32_t nalloc;
	/** The head index of elements which is free */
	uint32_t free_head;
	/** The tail index of elements which is free */
	uint32_t free_tail;
	/** The elements in the array */
	free_info_t elts[];
} fmempool_t, *fmempool_ptr_t;


fmempool_ptr_t new_fmempool(const char *pathname, int flags, mode_t mode, uint32_t elt_size, uint32_t nalloc);
fmempool_ptr_t load_fmempool(const char *pathname, int flags, mode_t mode);

void *fmempool_alloc(fmempool_ptr_t ptr, uint32_t nalloc);
void *fmempool_free(fmempool_ptr_t ptr, void *data, uint32_t nalloc);

int fmempool_sync(fmempool_ptr_t ptr);
int free_fmempool(fmempool_ptr_t ptr);

#endif /* _FILE_MEMPOOL_H_INCLUDE_ */

