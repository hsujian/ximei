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
	int rlen;
	int eno;
	char *buf;
} job_t;

struct Pipeline_t {
	unsigned max_job_num;
	unsigned pos;
	unsigned do_pos;
	int recv_buf_len;
	int send_buf_len;
	struct ev_loop *loop;
	job_t *jobs;
};

static void read_conn(EV_P_ struct ev_io *w, int revents)
{
	ev_io_stop(EV_A_ w);
	job_t *job = (job_t *)((char*)w - offsetof(struct job_t, w));
	Pipeline_t *_this = (Pipeline_t *)ev_userdata (EV_A);
	int idx = job - _this->jobs;
	job->rlen = socket_recv(w->fd, job->buf, _this->recv_buf_len);
	job->eno = errno;
	if (job->eno == EPIPE || (job->rlen < 1 && job->eno == EAGAIN)) {
		lingering_close( w->fd );
		WARNING_LOG("free job[%d]", idx);
		job->w.fd = -1;
		job->status = JOBS_NO_JOB;
		_this->pos = idx;
		return;
	}

	DEBUG_LOG("job[%d]->status=%d", idx, job->status);
	job->status = JOBS_REQUEST_IN;
}

static void accept_conn(EV_P_ struct ev_io *w, int revents)
{
	int s;
	unsigned i;
	int ncontinue;
	Pipeline_t *_this = (Pipeline_t *)ev_userdata (EV_A);
	while(1) {
		s = accept(w->fd, NULL, NULL);
		if (s < 0 && errno == EAGAIN) {
			return;
		}
		if (s < 0) {
			continue;
		}
		setnonblock(s);

		ncontinue = 0;
		for (i=_this->pos; i<_this->max_job_num; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_NO_JOB, JOBS_WAIT_REQUEST)) {
				ev_io_init(&(_this->jobs[i].w), read_conn, s, EV_READ);
				ev_io_start(EV_A_ &(_this->jobs[i].w));
				ncontinue = 1;
				_this->pos = i + 1;
				DEBUG_LOG("accept job[%d]->w.fd=%d", i, s);
				break;
			}
		}
		if (ncontinue) {
			continue;
		}
		for (i=0; i<_this->max_job_num; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_NO_JOB, JOBS_WAIT_REQUEST)) {
				ev_io_init(&(_this->jobs[i].w), read_conn, s, EV_READ);
				ev_io_start(EV_A_ &(_this->jobs[i].w));
				DEBUG_LOG("accept job[%d]->w.fd=%d", i, s);
				ncontinue = 1;
				_this->pos = i + 1;
				break;
			}
		}
		if (ncontinue) {
			continue;
		}
		lingering_close(s);
		WARNING_LOG("no idle client");
	}
}

int pipeline_fetch_item(Pipeline_t *_this, int *idx, int *sock, const char **pbuf, int *rlen)
{
	unsigned i;
	for (;;) {
		for (i=_this->do_pos; i<_this->max_job_num; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_REQUEST_IN, JOBS_DO_JOB)) {
				*sock = _this->jobs[i].w.fd;
				*idx = i;
				*pbuf = (const char *) _this->jobs[i].buf;
				*rlen = _this->jobs[i].rlen;
				_this->do_pos = i + 1;
				DEBUG_LOG("fetch client[%d]->io.fd=%d", *idx, *sock);
				return 0;
			}
		}
		for (i=0; i<_this->max_job_num; i++) {
			if (CAS(&(_this->jobs[i].status), JOBS_REQUEST_IN, JOBS_DO_JOB)) {
				*sock = _this->jobs[i].w.fd;
				*idx = i;
				*pbuf = (const char *) _this->jobs[i].buf;
				*rlen = _this->jobs[i].rlen;
				_this->do_pos = i + 1;
				DEBUG_LOG("fetch client[%d]->io.fd=%d", *idx, *sock);
				return 0;
			}
		}
		//for(i=2; i--;) {
			sched_yield();
		//}
	}
	return 0;
}

void pipeline_reset_item(Pipeline_t *_this, int idx, int keep_alive)
{
	if (idx < 0 || idx >= _this->max_job_num) {
		WARNING_LOG("idx[%d] not in [0, %d)", idx, _this->max_job_num);
		return;
	}
	_this->jobs[idx].status = JOBS_DO_DONE;
	if (keep_alive) {
		DEBUG_LOG("keep alive client[%d]->io.fd=%d", idx, _this->jobs[idx].w.fd);
		_this->jobs[idx].status = JOBS_WAIT_REQUEST;
		ev_io_start(_this->loop, &(_this->jobs[idx].w));
		return;
	}
	lingering_close( _this->jobs[idx].w.fd );
	DEBUG_LOG("free jobs[%d]", idx);
	_this->jobs[idx].w.fd = -1;
	_this->jobs[idx].status = JOBS_NO_JOB;
	_this->pos = idx;
}

int pipeline_listen(Pipeline_t *_this, const int fd)
{
	struct ev_io *w = (struct ev_io *)malloc(sizeof(struct ev_io));
	if (IS_NULL(w)) {
		return -1;
	}
	setnonblock(fd);
	ev_io_init(w, accept_conn, fd, EV_READ);
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

Pipeline_t *pipeline_creat(const int max_job_num, const int recv_buf_len, const int send_buf_len)
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
	_new->recv_buf_len = recv_buf_len;
	_new->send_buf_len = send_buf_len;
	_new->jobs = (job_t *)calloc(mnum, sizeof(job_t));
	if (IS_NULL(_new->jobs)) {
		free(_new);
		return NULL;
	}

	char *buf = (char *)malloc((recv_buf_len + send_buf_len) * mnum);
	if (IS_NULL(buf)) {
		free(_new->jobs);
		free(_new);
		return NULL;
	}
	int i;
	for (i=mnum; i--;) {
		_new->jobs[i].buf = buf + (recv_buf_len + send_buf_len) * i;
	}

	_new->loop = ev_loop_new (EVBACKEND_EPOLL);
	if (IS_NULL( _new->loop )) {
		free(buf);
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

