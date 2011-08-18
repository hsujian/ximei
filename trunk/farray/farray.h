#ifndef _FARRAY_H_INCLUDE
#define _FARRAY_H_INCLUDE

typedef struct{
	/** The amount of memory allocated for each element of the array */
	size_t elt_size;
	/** The number of active elements in the array */
	size_t nelts;
	/** The number of elements allocated in the array */
	size_t nalloc;
	/** The elements in the array */
	char elts[];
} farray_t, *farray_ptr_t;

farray_ptr_t new_farray(const char *pathname, int flags, mode_t mode, int block_size, int block_num);
farray_ptr_t load_farray(const char *pathname, int flags, mode_t mode, int block_size, int block_num);
int farray_sync(farray_ptr_t ptr);
int farray_free(farray_ptr_t ptr);

#endif /* _FARRAY_H_INCLUDE */
