/*knowledge: http://www.ibm.com/developerworks/cn/linux/l-cn-ppp/index6.html*/

#ifndef __MEMPOOL_H_
#define __MEMPOOL_H_

#include <pthread.h>

#ifdef __cplusplus
extern "C"{
#endif
/*
typedef struct memory_block_s {
	int malloc_size;
	unsigned short free_block_num;
	unsigned short first_free_unit;
	struct memory_block_s *next;
	char memory_start[1];
} memory_block_s;

struct memory_cache_s {
	pthread_mutex_t mutex;
	struct memory_block_s *p_block;
	int block_size;
	unsigned short block_num;
	unsigned short dynamic_num;
} ;
*/
typedef struct memory_cache_s memory_cache_s;

extern struct memory_cache_s * memory_cache_create(int block_size, int block_num, int dynamic_num);
extern void memory_cache_destroy(struct memory_cache_s* memory_cache);
extern void* memory_cache_malloc(struct memory_cache_s* memory_cache);
extern void memory_cache_free(struct memory_cache_s* memory_cache, void* memory);

#ifdef __cplusplus
}
#endif

#endif
