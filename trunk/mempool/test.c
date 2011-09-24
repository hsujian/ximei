#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "mempool.h"

#define MAX_TEST_ITEM	8000
#define TEST_BLOCK_SIZE	(1024*20)
int main(int argc, char **argv)
{
	int i = 0;
	int times = atoi(argv[1]);
	void *mm;
	memory_cache_s *mc = memory_cache_create(TEST_BLOCK_SIZE, 300, 100);
	//mm = memory_cache_malloc(mc);
	for (i=0; i<times; i++)
	{
		mm = memory_cache_malloc(mc);
		if (mm!=NULL)
			memory_cache_free(mc, mm);
	}
	
	memory_cache_destroy(mc);
	return 0;
}
