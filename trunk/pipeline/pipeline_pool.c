#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>

#include <ev.h>

#include "pipeline_pool.h"

#define CAS __sync_bool_compare_and_swap

#define IS_NULL(_ptr) (NULL == _ptr)

enum {
	JOBS_NO_JOB=0,
	JOBS_WAIT_REQUEST,
	JOBS_REQUEST_IN,
	JOBS_DO_JOB,
	JOBS_DO_DONE
};

typedef struct job_t {
	int status;
	struct ev_io w;
} job_t;

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
	Pipeline_t *_this = (Pipeline_t *)ev_userdata (EV_A);
	DEBUG_LOG("job[%d]->status=%d", job - _this->jobs, job->status);
	CAS(&(job->status), JOBS_WAIT_REQUEST, JOBS_REQUEST_IN);
	sched_yield();
}

static void accept_connection(EV_P_ struct ev_io *w, int revents)
{
	int s;
	while(1) {
		s = accept(w->fd, NULL, NULL);
		if (s < 0 && errno == EAGAIN) {
			return;
		}
		setnonblock(s);

		Pipeline_t *_this = (Pipeline_t *)ev_userdata (EV_A);
		int pos = _this->pos;
		int i = 0;
		int status = 0;
		if (pos < 0) {
			pos = 0;
		}
		for (i=pos; i<_this->max_job_num; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_NO_JOB, JOBS_WAIT_REQUEST)) {
				ev_io_init(&(_this->jobs[i].w), read_connection, s, EV_READ);
				ev_io_start(EV_A_ &(_this->jobs[i].w));
				status = 1;
				_this->pos = i + 1;
				DEBUG_LOG("accept job[%d]->w.fd=%d", i, s);
				break;
			}
		}
		if (!status) {
			if (pos > _this->max_job_num) {
				pos = _this->max_job_num;
			}
			for (i=0; i<pos; i++) {
				if (CAS(&(_this->jobs[i].status), JOBS_NO_JOB, JOBS_WAIT_REQUEST)) {
					ev_io_init(&(_this->jobs[i].w), read_connection, s, EV_READ);
					ev_io_start(EV_A_ &(_this->jobs[i].w));
					DEBUG_LOG("accept job[%d]->w.fd=%d", i, s);
					status = 1;
					_this->pos = i + 1;
					break;
				}
			}
		}
		if (!status) {
			lingering_close(s);
			WARNING_LOG("no idle client");
		}

	}
}

int pipeline_fetch_item(Pipeline_t *_this, int *index, int *sock)
{
	for (;;) {
		int i = 0;
		int pos = _this->do_pos;
		if (pos < 0) {
			pos = 0;
		}
		for (i=pos; i<_this->max_job_num; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_REQUEST_IN, JOBS_DO_JOB)) {
				*sock = _this->jobs[i].w.fd;
				*index = i;
				_this->do_pos = i + 1;
				DEBUG_LOG("fetch client[%d]->io.fd=%d", *index, *sock);
				return 0;
			}
		}
		if (pos > _this->max_job_num) {
			pos = _this->max_job_num;
		}
		for (i=0; i<pos; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_REQUEST_IN, JOBS_DO_JOB)) {
				*sock = _this->jobs[i].w.fd;
				*index = i;
				_this->do_pos = i + 1;
				DEBUG_LOG("fetch client[%d]->io.fd=%d", *index, *sock);
				return 0;
			}
		}
		//for(i=2; i--;) {
			sched_yield();
		//}
	}
	return 0;
}

void pipeline_reset_item(Pipeline_t *_this, int index, int keep_alive)
{
	if (index < 0 || index >= _this->max_job_num) {
		WARNING_LOG("index[%d] not in [0, %d)", index, _this->max_job_num);
		return;
	}
	_this->jobs[index].status = JOBS_DO_DONE;
	if (keep_alive) {
		DEBUG_LOG("keep alive client[%d]->io.fd=%d", index, _this->jobs[index].w.fd);
		//_this->jobs[index].status = JOBS_WAIT_REQUEST;
		return;
	}
	ev_io_stop(_this->loop, &(_this->jobs[index].w));
	lingering_close( _this->jobs[index].w.fd );
	WARNING_LOG("free client[%d]", index);
	_this->jobs[index].w.fd = -1;
	_this->jobs[index].status = JOBS_NO_JOB;
	_this->pos = index;
	//sched_yield();
}

int pipeline_listen(Pipeline_t *_this, const int fd)
{
	struct ev_io *w = (struct ev_io *)malloc(sizeof(struct ev_io));
	if (IS_NULL(w)) {
		return -1;
	}
	setnonblock(fd);
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
	_new->jobs = (job_t *)calloc(mnum, sizeof(job_t));
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

int pipeline_listen_port(Pipeline_t *_this, const int port)
{
	int fd = socket_listen_port(port);
	if (fd == -1) {
		return -1;
	}
	return pipeline_listen(_this, fd);
}

