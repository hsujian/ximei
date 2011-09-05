#ifndef __PIPELINE_POOL_H_
#define __PIPELINE_POOL_H_

#include <sys/uio.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct Pipeline_t Pipeline_t;

extern Pipeline_t *pipeline_creat(const int max_job_num);

extern int pipeline_del(Pipeline_t *pl);

extern int pipeline_fetch_item(Pipeline_t *pl, int *index, int *sock);

extern void pipeline_reset_item(Pipeline_t *pl, int index, bool keep_alive);

extern void pipeline_run(Pipeline_t *pl);

extern int pipeline_listen(Pipeline_t *_this, const int fd);

#endif
