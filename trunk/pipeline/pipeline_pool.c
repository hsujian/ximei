#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

#include <ev.h>

#include "pipeline_pool.h"

#include "ty_log.h"

#define CAS __sync_bool_compare_and_swap

#define IS_NULL(_ptr) (NULL == _ptr)

enum {
	JOBS_FREE=0,
	JOBS_WAIT_READ,
	JOBS_CAN_READ,
	JOBS_INIT,
	JOBS_DO_READ,
	JOBS_DO_DONE
};

#define IS_CLIENT_IN_QUEUE(_C) (AS_CLIENT_QUEUE == _C->status)

typedef struct job_t {
	int status;
	struct ev_io w;
} job_t;

typedef struct Pipeline_t Pipeline_t;

struct Pipeline_t {
	int max_job_num;
	int pos;
	int do_pos;
	struct ev_loop *loop;
	job_t *jobs;
};

static void read_connection(EV_P_ struct ev_io *w, int revents)
{
	job_t *job = (job_t *)((char*)w - offsetof(struct job_t, w));
	CAS(&(job->status), JOBS_WAIT_READ, JOBS_CAN_READ);
}

static void accept_connection(EV_P_ struct ev_io *w, int revents)
{
	int s;
	do {
		s = accept(w->fd, NULL, NULL);
		if (s < 0) {
			return;
		}
		setnonblock(s);

		Pipeline_t *_this = ev_userdata (EV_A);
		int pos = _this->pos;
		int i = 0;
		int status = 0;
		for (i=pos; i<_this->max_job_num; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_FREE, JOBS_INIT)) {
				ev_io_init(&(_this->jobs[i].w), read_connection, s, EV_READ);
				ev_io_start(EV_A_ &(_this->jobs[i].w));
				status = 1;
				_this->pos = i + 1;
				//DEBUG_LOG("accept job[%d]->w.fd=%d", i, s);
				break;
			}
		}
		if (!status) {
			for (i=0; i<pos; i++) {
				if (CAS(&(_this->jobs[i].status), JOBS_FREE, JOBS_INIT)) {
					ev_io_init(&(_this->jobs[i].w), read_connection, s, EV_READ);
					ev_io_start(EV_A_ &(_this->jobs[i].w));
					//DEBUG_LOG("accept job[%d]->w.fd=%d", i, s);
					status = 1;
					_this->pos = i + 1;
					break;
				}
			}
		}
		if (!status) {
			as_lingering_close(s);
			WARNING_LOG("no idle client");
		}

	} while ( s > -1);
}

int pipeline_fetch_item(Pipeline_t *_this, int *index, int *sock)
{
	for (;;) {
		int i = 0;
		int pos = _this->do_pos;
		for (i=pos; i<_this->max_job_num; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_CAN_READ, JOBS_DO_READ)) {
				*sock = _this->jobs[i].w.fd;
				*index = i;
				_this->do_pos = i + 1;
				WARNING_LOG("fetch client[%d]->io.fd=%d", *index, *sock);
				return 0;
			}
		}
		for (i=0; i<pos; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_CAN_READ, JOBS_DO_READ)) {
				*sock = _this->jobs[i].w.fd;
				*index = i;
				_this->do_pos = i + 1;
				WARNING_LOG("fetch client[%d]->io.fd=%d", *index, *sock);
				return 0;
			}
		}
		sched_yield();
	}
	return 0;
}

void pipeline_reset_item(Pipeline_t *_this, int index, bool keep_alive)
{
	if (index < 0 || index >= _this->max_job_num) {
		WARNING_LOG("index[%d] not in [0, %d)", index, _this->max_job_num);
		return;
	}
	_this->jobs[index].status = JOBS_DO_DONE;
	if (keep_alive) {
		DEBUG_LOG("keep alive client[%d]->io.fd=%d", index, _this->jobs[index].w.fd);
		_this->jobs[index].status = JOBS_WAIT_READ;
		return;
	}
	ev_io_stop(_this->loop, &(_this->jobs[index].w));
	as_lingering_close( _this->jobs[index].w.fd );
	WARNING_LOG("free client[%d]", index);
	_this->jobs[index].w.fd = -1;
	_this->jobs[index].status = JOBS_FREE;
	_this->pos = index;
}

int pipeline_listen(Pipeline_t *_this, const int fd)
{
	setnonblock(fd);
	struct ev_io *w = malloc(sizeof(struct ev_io));
	ev_io_init(w, accept_connection, fd, EV_READ);
	ev_io_start(_this->loop, w);
	return 0;
}

void pipeline_run(Pipeline_t *_this)
{
	ev_run (_this->loop, 0);
}

int pipeline_del(Pipeline_t *_this)
{
	if (IS_NULL( _this )) {
		return -1;
	}
	ev_break (_this->loop, EVBREAK_ALL);
	free(_this->jobs);
	free(_this);
	return 0;
}

Pipeline_t *pipeline_creat(const int max_job_num)
{
	int mnum = 0;
	if (max_job_num < 1) {
		mnum = 1024;
	} else {
		mnum = max_job_num;
	}

	Pipeline_t *_new = (Pipeline_t *) calloc ( 1, sizeof(Pipeline_t) );
	if (IS_NULL( _new )) {
		return NULL;
	}
	_new->max_job_num = mnum;
	_new->jobs = calloc(mnum, sizeof(job_t));
	if (IS_NULL(_new->jobs)) {
		free(_new);
		return NULL;
	}

	_new->loop = ev_loop_new (EVBACKEND_EPOLL);
	if (IS_NULL( _new->loop )) {
		free(_new->jobs);
		free(_new);
		return NULL;
	}

	ev_set_userdata(_new->loop, _new);

	return _new;
}

