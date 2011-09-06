#ifndef __PIPELINE_POOL_H_
#define __PIPELINE_POOL_H_

#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Pipeline_t Pipeline_t;

Pipeline_t *pipeline_creat(const int max_job_num);

int pipeline_del(Pipeline_t *pl);

int pipeline_fetch_item(Pipeline_t *pl, int *index, int *sock);

void pipeline_reset_item(Pipeline_t *pl, int index, int keep_alive);

void pipeline_run(Pipeline_t *pl);

int pipeline_listen(Pipeline_t *_this, const int fd);
int pipeline_listen_port(Pipeline_t *_this, const int port);

#ifdef __cplusplus
}
#endif

#endif
