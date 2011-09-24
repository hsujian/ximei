#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "mempool.h"

#define MAX_TEST_ITEM	30
#define MAX_TEST_THREAD	10
#define TEST_BLOCK_SIZE	10240

void thread(int number);
MEMORY_CACHE_S *mem_cache = NULL;
int main(void)
{
	//printf("This is the main process.\n");
	
	pthread_t id[MAX_TEST_THREAD];
	mem_cache = memory_cache_create(TEST_BLOCK_SIZE, 10, 10);
	int ret;
	int i=0;
	for (; i<MAX_TEST_THREAD; i++)
	{
		ret=pthread_create(&id[i],NULL,(void *) thread,(int*)i);
		if(ret!=0)
		{
			printf ("Create pthread error!\n");
			//exit (1);
		}
	}
	/*ret=pthread_create(&id[1],NULL,(void *) thread,(int*)10);
	if(ret!=0)
	{
		printf ("Create pthread error!\n");
		exit (1);
	}*/
	pthread_join(id[0],NULL);
	for (i=0; i<MAX_TEST_THREAD; i++)
	{
		pthread_join(id[i],NULL);
	}
	/*pthread_join(id[1],NULL);*/
	memory_cache_destroy(mem_cache);
	return (0);
}

void thread(int number)
{
	//printf("This is the %d pthread.\n",number);

	/*MEMORY_CACHE_S *mem_cache = memory_cache_create(TEST_BLOCK_SIZE, 5, 5);*/
	int i = 0;
	int ret = 0;
	long last_time = 0;
	long cur_time = 0;
	struct timeval tv;
	
	char *test[2][MAX_TEST_ITEM];
	int sumTime[2] = {0,0};
	char *status[2] = {"SUCC","FAIL"};
	//MEMORY_CACHE_S *mem_cache = memory_cache_create(TEST_BLOCK_SIZE, 30, 30);
	//printf("malloc memory\n");
	printf("    id%20s%20s\n", "mempool time", "normal time");
	for (i=0; i<MAX_TEST_ITEM; i++)
	{
		//printf("%5d ", i);
		gettimeofday (&tv , NULL);
		last_time = tv.tv_usec;
		test[0][i] = (char*)memory_cache_malloc(mem_cache);
		if (NULL == test[0][i])
		{
			ret = 1;
		}
		else
		{
			ret = 0;
		}
		gettimeofday (&tv , NULL);
		cur_time = tv.tv_usec;
		printf("%15d %s", (int)(cur_time - last_time), status[ret]);
		sumTime[0] += (int)(cur_time - last_time);
		snprintf(test[0][i], TEST_BLOCK_SIZE, "%d_cache_%d %d %s", number, i, (int)(cur_time - last_time), status[ret]);
		//printf("%20s %#x", test[0][i], (unsigned int)test[0][i]);
		gettimeofday (&tv , NULL);
		last_time = tv.tv_usec;
		test[1][i] = (char*)malloc(TEST_BLOCK_SIZE);
		if (NULL == test[1][i])
		{
			ret = 1;
		}
		else
		{
			ret = 0;
		}
		gettimeofday (&tv , NULL);
		cur_time = tv.tv_usec;
		printf("%15d %s\n", (int)(cur_time - last_time), status[ret]);
		sumTime[1] += (int)(cur_time - last_time);
		snprintf(test[1][i], TEST_BLOCK_SIZE, "%d_array_%d", number, i);

	}
	printf("thread %d:%d %d save:%f%%\n", number, sumTime[0], sumTime[1], (float)(sumTime[1]-sumTime[0])/sumTime[1]*100);
	
	/*printf("printf test\n");
	printf("   id%30s%30s\n", "mempool test address", "normal test address");
	for (i=0; i<MAX_TEST_ITEM; i++)
	{
		printf("%5d", i);
		printf("%20s %#x", test[0][i], (unsigned int)test[0][i]);
		printf("%20s %#x\n", test[1][i], (unsigned int)test[1][i]);
	}
	printf("\n\n\n");*/
	//sleep(number);
	//if (number < MAX_TEST_THREAD-1)
	//{
	//	return;
	//}
	/*
	struct memory_block_s *p_cur = mem_cache->p_block;
	char *pchar = NULL;
	while (p_cur != NULL)
	{
		//printf("%s\n", (char*)p_cur->memory_start);
		pchar = (char*)p_cur->memory_start;
		ret = 0;
		i=0;
		while (i<p_cur->malloc_size)
		{
			if (*(p_cur->memory_start+i)==0)
			{
				if (ret==1)
				{
					i++;
					if (i%TEST_BLOCK_SIZE==0)
					{
						printf(" EOB\n");
					}
					continue;
				}
				ret = 1;
				i++;
				if (i%TEST_BLOCK_SIZE==0)
				{
					printf(" EOB\n");
				}
			}
			else
			{
				printf("%c", *((char*)p_cur->memory_start+i++));
				ret = 0;
				if (i%TEST_BLOCK_SIZE==0)
				{
					printf(" EOB\n");
				}
			}
		}
		printf("\n");
		p_cur = p_cur->next;

	}
	printf("free memory\n");
	printf("   id%20s%20s\n", "mempool time", "normal time");*/
	sumTime[0]=0;sumTime[1]=0;
	for (i=0; i<MAX_TEST_ITEM; i++)
	{
		//printf("%5d", i);
		gettimeofday (&tv , NULL);
		last_time = tv.tv_usec;
		memory_cache_free(mem_cache, test[0][i]);
		gettimeofday (&tv , NULL);
		cur_time = tv.tv_usec;
		//printf("%20d", (int)(cur_time - last_time));
		sumTime[0] += (int)(cur_time - last_time);
		gettimeofday (&tv , NULL);
		last_time = tv.tv_usec;
		free(test[1][i]);
		gettimeofday (&tv , NULL);
		cur_time = tv.tv_usec;
		//printf("%20d\n", (int)(cur_time - last_time));
		sumTime[1] += (int)(cur_time - last_time);
	}
	printf("total:%20d%20d\tsave:%f%%\n", sumTime[0], sumTime[1], (float)(sumTime[1]-sumTime[0])/sumTime[1]*100);
	
	//printf("begin memory_cache_destroy\n");
	//memory_cache_destroy(mem_cache);
	//printf("memory_cache_destroy over\n");
	/*memory_cache_destroy(mem_cache);*/
	return;
}
