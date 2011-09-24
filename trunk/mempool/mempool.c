#include <malloc.h>
#include "mempool.h"

typedef struct memory_block_s {
	int malloc_size;
	unsigned short free_block_num;
	unsigned short first_free_unit;
	struct memory_block_s *next;
	char memory_start[1];
} memory_block_s;

struct memory_cache_s{
	pthread_mutex_t mutex;
	struct memory_block_s *p_block; 
	int block_size;
	unsigned short block_num;
	unsigned short dynamic_num;
} ;

#ifndef NDEBUG
#include <stdio.h>
#include <stdlib.h>
#define die(msg, ...) do{fprintf(stderr, msg, ##__VA_ARGS__);fprintf(stderr, "\n");exit(-1);}while(0)
#define fatal(msg, ...) do{fprintf(stderr, msg, ##__VA_ARGS__);fprintf(stderr, "\n");}while(0)
#define warn(msg, ...) do{fprintf(stderr, msg, ##__VA_ARGS__);fprintf(stderr, "\n");}while(0)
#define debug(msg, ...) do{fprintf(stderr, msg, ##__VA_ARGS__);fprintf(stderr, "\n");}while(0)

static int mallocTime=0;
static int mcmTime=0;
static int freeTime=0;
static int mcfTime=0;
#define MALLOC(s) (mallocTime++, malloc(s))
#define FREE(s) (freeTime++, free(s))

void mf()
{
	debug("mallocTime:%d freeTime:%d mcMallocTime:%d mcFreeTime:%d", 
			mallocTime, freeTime, mcmTime, mcfTime);
}

#else
#define die(m)
#define fatal(m)
#define warn(m)
#define debug(m)
#define MALLOC(s) malloc(s)
#define FREE(s) free(s)
#endif

/*
1 ��ʼ���ڴ��
 param  block_size : һ���ڴ���С
     	block_num: �״η����ڴ�����
      	dynamic_num: ���ڴ���ڴ治��ʱ����̬���ӵ��ڴ�����
 return �ڴ��
 */

struct memory_cache_s * memory_cache_create(int block_size, int block_num, int dynamic_num)
{
	memory_cache_s *p_memory_cache_obj = (memory_cache_s *)MALLOC(sizeof(memory_cache_s));
	if (NULL == p_memory_cache_obj)
	{
		return NULL;
	}
	if (0 != pthread_mutex_init(&p_memory_cache_obj->mutex, NULL))
	{
		FREE (p_memory_cache_obj);
		return NULL;
	}
	p_memory_cache_obj->block_num = block_num;
	p_memory_cache_obj->dynamic_num = dynamic_num;
	p_memory_cache_obj->p_block = NULL;
	p_memory_cache_obj->block_size = (block_size < 2)?2:block_size;

	p_memory_cache_obj->p_block = (memory_block_s *)MALLOC(sizeof(memory_block_s) + p_memory_cache_obj->block_size * block_num);
	if (NULL != p_memory_cache_obj->p_block)
	{
		debug("block malloc once");
		memory_block_s *p_block = p_memory_cache_obj->p_block;
		p_block->malloc_size = p_memory_cache_obj->block_size * block_num;
		p_block->free_block_num = block_num;
		p_block->first_free_unit = 0;
		p_block->next = NULL;
		unsigned short i = 0;
		for (; i<block_num; i++)
		{
			*(unsigned short*)(p_block->memory_start + p_memory_cache_obj->block_size * i) = i+1;
		}
	}
#ifndef NDEBUG
	atexit(mf);
#endif

	return p_memory_cache_obj;
}
/*
2 �ͷ��ڴ��

  param  memory_cache ���ڴ��
 */
void memory_cache_destroy(struct memory_cache_s* memory_cache)
{
	struct memory_block_s *pblock = memory_cache->p_block;
	pthread_mutex_destroy(&memory_cache->mutex);
	while (NULL != pblock)
	{
		memory_cache->p_block = pblock->next;
		FREE(pblock);
		pblock = memory_cache->p_block;
	}
	FREE (memory_cache);
}
/*
3 �����ڴ�

  param  memory_cache ���ڴ��
 
  return һ���ڴ��
 */
void *memory_cache_malloc(struct memory_cache_s *memory_cache)
{
	unsigned short i = 0;
	unsigned short j = 0;
	memory_block_s *p_cur_block = NULL;
	pthread_mutex_lock(&memory_cache->mutex);

	if (NULL == memory_cache->p_block)
	{
		p_cur_block = (memory_block_s *)MALLOC(sizeof(memory_block_s) + memory_cache->block_size * memory_cache->block_num);
		debug("malloc block once");
		if (NULL == p_cur_block)
		{
			pthread_mutex_unlock(&memory_cache->mutex);
			return NULL;
		}

		p_cur_block->malloc_size = memory_cache->block_size * memory_cache->block_num;
		p_cur_block->free_block_num = memory_cache->block_num-1;
		p_cur_block->first_free_unit = 1;
		p_cur_block->next = NULL;
		j = memory_cache->block_num;
		for (i=1; i<j; i++)
		{
			*(unsigned short*)(p_cur_block->memory_start + memory_cache->block_size * i) = i+1;
		}
		if (NULL != memory_cache->p_block)
		{
			p_cur_block->next = memory_cache->p_block->next;
		}
		memory_cache->p_block = p_cur_block;
		pthread_mutex_unlock(&memory_cache->mutex);
#ifndef NDEBUG
		mcmTime++;
#endif
		return p_cur_block->memory_start;
	}

	p_cur_block = memory_cache->p_block;
	while (NULL != p_cur_block && p_cur_block->free_block_num < 1)
	{
		p_cur_block = p_cur_block->next;
	}

	if (NULL != p_cur_block)
	{
		void *pfree = p_cur_block->memory_start+p_cur_block->first_free_unit * memory_cache->block_size;
		p_cur_block->free_block_num--;
		if (p_cur_block->free_block_num > 0)
		{
			p_cur_block->first_free_unit = *(unsigned short*)pfree;
		}
		pthread_mutex_unlock(&memory_cache->mutex);
#ifndef NDEBUG
		mcmTime++;
#endif
		return pfree;
	}
	else
	{
		pthread_mutex_unlock(&memory_cache->mutex);

		p_cur_block = (memory_block_s *)MALLOC(sizeof(memory_block_s) + memory_cache->block_size * memory_cache->dynamic_num);
		debug("malloc block once");
		if (NULL == p_cur_block)
		{
			return NULL;
		}
		p_cur_block->malloc_size = memory_cache->block_size * memory_cache->dynamic_num;
		p_cur_block->free_block_num = memory_cache->dynamic_num-1;
		p_cur_block->first_free_unit = 1;
		j = memory_cache->dynamic_num;
		for (i=1; i<j; i++)
		{
			*(unsigned short*)(p_cur_block->memory_start + memory_cache->block_size * i) = i+1;
		}

		pthread_mutex_lock(&memory_cache->mutex);
		p_cur_block->next = memory_cache->p_block;
		memory_cache->p_block = p_cur_block;
		pthread_mutex_unlock(&memory_cache->mutex);

#ifndef NDEBUG
		mcmTime++;
#endif
		return p_cur_block->memory_start;
	}
}

/*
4 �ͷ��ڴ�
  param  memory_cache ���ڴ��
        memory: ��Ҫ�ͷŵ��ڴ��
 */ 
void memory_cache_free(struct memory_cache_s *memory_cache, void *memory)
{
#ifndef NDEBUG
	mcfTime++;
#endif
	pthread_mutex_lock(&memory_cache->mutex);
	memory_block_s *p_cur_block = memory_cache->p_block;
	if (NULL == p_cur_block)
	{
		pthread_mutex_unlock(&memory_cache->mutex);
		FREE (memory);
		debug("memory_cache->p_cur_block==NULL then free");
		return;
	}

	while ( ((unsigned long)p_cur_block->memory_start > (unsigned long)memory) ||
			((unsigned long)memory >= ((unsigned long)p_cur_block->memory_start + p_cur_block->malloc_size)) )
	{
		p_cur_block = p_cur_block->next;
		if (NULL == p_cur_block)
		{
			pthread_mutex_unlock(&memory_cache->mutex);
			FREE(memory);
			debug("p_cur_block==NULL then free");
			return;
		}
	}

	*(unsigned short*)memory = p_cur_block->first_free_unit;
	p_cur_block->free_block_num++;
	p_cur_block->first_free_unit = (unsigned short)(((unsigned long)memory - (unsigned long)(p_cur_block->memory_start)) / memory_cache->block_size);

	/*�����ǰ�ڴ�ȫ��δ�������ȥ����ѵ�ǰ�ڴ���ͷ�*/
	/*��������ں�����ڴ棬���Բ��ͷţ��������ڴ�������ֹ��ͷŵ�ʱ�򣬲��ͷſռ�*/
	if (p_cur_block->free_block_num * memory_cache->block_size == p_cur_block->malloc_size
			&& memory_cache->p_block == p_cur_block
			&& p_cur_block->next != NULL/*���һ���ڴ�鲻����*/)
	{
		memory_cache->p_block = p_cur_block->next;
		FREE (p_cur_block);
		debug("block free once");
	}
	pthread_mutex_unlock(&memory_cache->mutex);
	return;
}

