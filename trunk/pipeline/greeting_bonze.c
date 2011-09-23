#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <aio.h>
#include <sched.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#include "greeting_bonze.h"

#define CAS __sync_bool_compare_and_swap

enum {
	JOBS_NO_JOB=0,
	JOBS_WAIT_REQUEST,
	JOBS_REQUEST_IN,
	JOBS_DO_JOB,
	JOBS_DO_DONE,
	JOBS_WAIT_SEND_OFF,
	JOBS_SEND_DONE
};

typedef struct guest_t {
	volatile int status;
	int in_len;
	int out_len;
	int eno;
	struct aiocb cb;
	greeting_bonze_t *gb;
	char *in_buf;
	char *out_buf;
} guest_t;

struct greeting_bonze_t {
	volatile int num;
	int capacity;
	int listen;
	volatile int last_fd;
	int in_size;
	int out_size;
	guest_t *guests;
	char *buf;
	guest_deal_fn guest_fn;
};

int greeting_bonze_get_in_len(greeting_bonze_t *gb, int fd)
{
	return gb->guests[fd].in_len;
}

int greeting_bonze_get_out_len(greeting_bonze_t *gb, int fd)
{
	return gb->guests[fd].out_len;
}

void greeting_bonze_set_in_len(greeting_bonze_t *gb, int fd, int len)
{
	gb->guests[fd].in_len = len;
}

void greeting_bonze_set_out_len(greeting_bonze_t *gb, int fd, int len)
{
	gb->guests[fd].out_len = len;
}

char *greeting_bonze_get_in_buf(greeting_bonze_t *gb, int fd)
{
	return gb->guests[fd].in_buf;
}

char *greeting_bonze_get_out_buf(greeting_bonze_t *gb, int fd)
{
	return gb->guests[fd].out_buf;
}

void greeting_bonze_set_guest_fn(greeting_bonze_t *gb, guest_deal_fn fn)
{
	gb->guest_fn = fn;
}

static int greeting_bonze_init_read(greeting_bonze_t *gb, const int fd);

int greeting_bonze_deal(greeting_bonze_t *gb, int *fd, const char **pbuf, int *rlen, const char **out_buf)
{
	for (;;) {
		while (gb->last_fd < 0) {
			sched_yield();
		}
		int i = gb->last_fd;
		if (i < 0) {
			i = 0;
		}
		for (; i<gb->capacity; i++) {
			if (CAS(&(gb->guests[i].status), JOBS_REQUEST_IN, JOBS_DO_JOB)) {
				*fd = i;
				*pbuf = (const char *) gb->guests[i].in_buf;
				*rlen = gb->guests[i].in_len;
				*out_buf = gb->guests[i].out_buf;
				gb->last_fd = i + 1;
				//DEBUG_LOG("request deal %d/%d\n", i, gb->guests[i].cb.aio_fildes);
				return 0;
			}
		}
		gb->last_fd = -1;
	}
	return 0;
}

static void greeted(sigval_t sigval)
{
	int len;
	struct aiocb *req = (struct aiocb *)sigval.sival_ptr;
	guest_t *g = (guest_t *)((char *)req - offsetof(guest_t, cb));
	/* Did the request complete? */
	int ret = aio_error( req );
	if (ret != 0) {
		g->eno = errno;
		DEBUG_LOG("request err fd=%d errno=%d ret=%d errmsg=%s\n", req->aio_fildes, g->eno, ret, strerror(g->eno));
		lingering_close(req->aio_fildes);
		return;
	}
	/* Request completed successfully, get the return status */
	len = aio_return( req );
	switch(g->status) {
		case JOBS_WAIT_REQUEST:
			g->in_len = len;
			g->gb->last_fd = req->aio_fildes;
			DEBUG_LOG("ret=%d request in %d rlen=%d\n", ret, req->aio_fildes, len);
			if (g->gb->guest_fn) {
				g->status = JOBS_DO_JOB;
				(*g->gb->guest_fn)(g->gb, req->aio_fildes);
			} else {
				g->status = JOBS_REQUEST_IN;
			}
			break;
		case JOBS_WAIT_SEND_OFF:
			g->status = JOBS_SEND_DONE;
			DEBUG_LOG("ret=%d request send done %d wlen=%d/%d\n", ret, req->aio_fildes, len, g->out_len);
			greeting_bonze_init_read(g->gb, req->aio_fildes);
			break;
		default:
			break;
	}
	return;
}

int greeting_bonze_listen(greeting_bonze_t *gb, const int fd)
{
	if (!gb) {
		return -1;
	}
	DEBUG_LOG("listen fd %d\n", fd);
	gb->listen = fd;
	return 0;
}

static int greeting_bonze_init_read(greeting_bonze_t *gb, const int fd)
{
	assert(gb);
	assert(fd < gb->capacity);

	struct aiocb *pcb = & gb->guests[fd].cb;
	/* Set up the AIO request */
	memset(pcb, 0, sizeof(struct aiocb));
	pcb->aio_fildes = fd;
	pcb->aio_buf = gb->guests[fd].in_buf;
	pcb->aio_nbytes = gb->in_size;
	pcb->aio_offset = 0;
	/* Link the AIO request with a thread callback */
	pcb->aio_sigevent.sigev_notify = SIGEV_THREAD;
	pcb->aio_sigevent.sigev_notify_function = greeted;
	pcb->aio_sigevent.sigev_notify_attributes = NULL;
	pcb->aio_sigevent.sigev_value.sival_ptr = pcb;

	gb->guests[fd].status = JOBS_WAIT_REQUEST;
	DEBUG_LOG("wait request %d\n", fd);
	return aio_read(pcb);
}

int greeting_bonze_send_off(greeting_bonze_t *gb, int fd, int out_len)
{
	assert(gb);
	assert(fd < gb->capacity);
	if (fd < 0 || fd >= gb->capacity) {
		WARNING_LOG("fd[%d] not in [0, %d)\n", fd, gb->capacity);
		return -1;
	}
	gb->guests[fd].status = JOBS_DO_DONE;
	if (out_len > gb->out_size) {
		out_len = gb->out_size;
	}
	gb->guests[fd].out_len = out_len;

	struct aiocb *pcb = & gb->guests[fd].cb;
	/* Set up the AIO request */
	memset(pcb, 0, sizeof(struct aiocb));
	pcb->aio_fildes = fd;
	pcb->aio_buf = gb->guests[fd].out_buf;
	pcb->aio_nbytes = out_len;
	pcb->aio_offset = 0;
	/* Link the AIO request with a thread callback */
	pcb->aio_sigevent.sigev_notify = SIGEV_THREAD;
	pcb->aio_sigevent.sigev_notify_function = greeted;
	pcb->aio_sigevent.sigev_notify_attributes = NULL;
	pcb->aio_sigevent.sigev_value.sival_ptr = pcb;

	gb->guests[fd].status = JOBS_WAIT_SEND_OFF;
	DEBUG_LOG("request wait send off fd=%d wlen=%d\n", fd, out_len);
	return aio_write(pcb);
}

int greeting_bonze_do(greeting_bonze_t *gb)
{
	if (!gb) {
		return -1;
	}
	int s;
	DEBUG_LOG("do listen fd %d\n", gb->listen);
	while(1) {
		s = accept(gb->listen, NULL, NULL);
		if (s < 0) {
			if (errno == EAGAIN) {
				int ret = 0;
				do {
					ret = wait_for_io_or_timeout(gb->listen, 1, -1);
					if (ret == -1) {
						DEBUG_LOG("wait for io timeout fail %d %s\n", errno, strerror(errno));
						return -1;
					}
				} while (ret == 0);
			} else {
				DEBUG_LOG("accept fail %d %s\n", errno, strerror(errno));
				return -1;
			}
		}
		//setnonblock(s);
		if (s >= gb->capacity) {
			lingering_close(s);
			WARNING_LOG("out of capacity fd=%d/%d\n", s, gb->capacity);
			continue;
		}
		DEBUG_LOG("accept %d\n", s);
		greeting_bonze_init_read(gb, s);
	}
}

void greeting_bonze_del(greeting_bonze_t *gb)
{
	if (! gb) {
		return;
	}
	if (gb->buf) {
		free(gb->buf);
	}
	if (gb->guests) {
		free(gb->guests);
	}
	free(gb);
	return;
}

greeting_bonze_t *greeting_bonze_new(const int capacity, const int in_size, const int out_size)
{
	int mnum = 0;
	if (capacity < 1) {
		mnum = 1024;
	} else {
		mnum = capacity;
	}

	greeting_bonze_t *_new = (greeting_bonze_t *) calloc ( 1, sizeof(greeting_bonze_t) );
	if (! _new) {
		return NULL;
	}
	_new->capacity = mnum;
	_new->last_fd = -1;
	_new->in_size = in_size;
	_new->out_size = out_size;
	_new->guests = (guest_t *)calloc(mnum, sizeof(guest_t));
	if (! _new->guests) {
		greeting_bonze_del(_new);
		return NULL;
	}
	char *buf = (char *)malloc((in_size + out_size) * mnum);
	if (! buf) {
		greeting_bonze_del(_new);
		return NULL;
	}
	int i;
	for (i=mnum; i--;) {
		_new->guests[i].gb = _new;
		_new->guests[i].in_buf = buf + (in_size + out_size) * i;
		_new->guests[i].out_buf = _new->guests[i].in_buf + in_size;
	}
	_new->buf = buf;
	DEBUG_LOG("new greeting_bonze c%d in%d out%d\n", capacity, in_size, out_size);
	return _new;
}

int greeting_bonze_listen_port(greeting_bonze_t *gb, const int port)
{
	if (gb == NULL) {
		return -1;
	}
	int fd = socket_tcplisten_port(port);
	if (fd == -1) {
		return -1;
	}
	DEBUG_LOG("listen port %d\n", port);
	return greeting_bonze_listen(gb, fd);
}

