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

#include "as_pending_pool.h"
#include "ty_log.h"

#define IS_NULL(_ptr) (NULL == _ptr)

typedef struct Client_item_t Client_item_t;

enum {
	AS_CLIENT_IDLE=0,
	AS_CLIENT_QUEUE
};

#define IS_CLIENT_IN_QUEUE(_C) (AS_CLIENT_QUEUE == _C->status)

struct Client_item_t {
	struct ev_io io;
	int status;
	Client_item_t *next;
};

typedef struct Pending_handle_t Pending_handle_t;

struct Pending_handle_t {
	int max_client_num;
	Client_item_t *idle;
	Client_item_t *work;
	Client_item_t *wtail;

	pthread_mutex_t m_idle_mutex;
	pthread_mutex_t m_mutex;
	pthread_cond_t m_cond;

	struct ev_loop *loop;
	struct ev_io listen_w;
	Client_item_t all_client[];
};

static Pending_handle_t *g_handle = NULL;


static inline int as_lingering_close( int fd );

static inline int setnonblock(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		return fcntl(fd, F_SETFL, O_NONBLOCK);
	}
	if (!(flags & O_NONBLOCK)) {
		flags |= O_NONBLOCK;
		return fcntl(fd, F_SETFL, flags);
	}
	return 0;
}

static inline Client_item_t *client_idle_pop()
{
	Pending_handle_t *_this = g_handle;
	if (IS_NULL( _this->idle )) {
		return NULL;
	}
	pthread_mutex_lock (&_this->m_idle_mutex);
	Client_item_t *client = _this->idle;
	_this->idle = _this->idle->next;
	pthread_mutex_unlock (&_this->m_idle_mutex);
	return client;
}

static inline void client_idle_push(Client_item_t *client)
{
	Pending_handle_t *_this = g_handle;
	pthread_mutex_lock (&_this->m_idle_mutex);
	client->next = _this->idle;
	_this->idle = client;
	pthread_mutex_unlock (&_this->m_idle_mutex);
}

static inline void client_work_ptail(Pending_handle_t *_this, Client_item_t *client)
{
	if (IS_CLIENT_IN_QUEUE(client)) {
		return;
	}
	pthread_mutex_lock (&_this->m_mutex);
	if (IS_CLIENT_IN_QUEUE(client)) {
		pthread_mutex_unlock (&_this->m_mutex);
		return;
	}
	client->status = AS_CLIENT_QUEUE;
	client->next = NULL;
	if (IS_NULL( _this->work )) {
		_this->work = client;
		if (!IS_NULL( _this->wtail )) {
			WARNING_LOG("err here");
		}
		_this->wtail = _this->work;
	} else {
		if (!IS_NULL( _this->wtail )) {
			_this->wtail->next = client;
		}
		_this->wtail = client;
	}
	pthread_cond_signal (&_this->m_cond);
	pthread_mutex_unlock (&_this->m_mutex);
}

static inline Client_item_t *client_work_ghead(Pending_handle_t *_this)
{
	Client_item_t *client = NULL;
	pthread_mutex_lock (&_this->m_mutex);
	while (IS_NULL( _this->work )) {
		pthread_cond_wait(&_this->m_cond, &_this->m_mutex);
	}

	client = _this->work;
	_this->work = _this->work->next;
	if (IS_NULL( _this->work )) {
		_this->wtail = NULL;
	}

	pthread_mutex_unlock (&_this->m_mutex);
	return client;
}

static void read_connection(EV_P_ struct ev_io *w, int revents)
{
	Client_item_t *client = (Client_item_t *)((char*)w - offsetof(struct Client_item_t, io));
	if (EV_ERROR & revents) {
		WARNING_LOG("catch a EV_ERROR client[%d] w->fd=%d", client - g_handle->all_client, w->fd);
		if (w->fd != -1) {
			ev_io_stop(EV_A_ w);
			as_lingering_close( client->io.fd );
			client->io.fd = -1;
		}
		client_idle_push(client);
		return;
	}

	if (IS_CLIENT_IN_QUEUE( client )) {
		return;
	}
	client_work_ptail(g_handle, client);
}

static inline int as_lingering_close( int fd )
{
	char buf[512];
	int rv;
	shutdown(fd, SHUT_WR);
	do {
		rv = read(fd, buf, sizeof(buf));
		if (rv == 0) {
			break;
		}
		if (rv == -1) {
			break;
		}
		WARNING_LOG("lc %d rv %d[%d]", fd, rv, errno);
	} while (rv > 0);
	return close(fd);
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

		Client_item_t *client = client_idle_pop();
		if (IS_NULL(client)) {
			as_lingering_close(s);
			WARNING_LOG("no idle client");
			return;
		}
		client->next = NULL;
		client->status = AS_CLIENT_IDLE;

		//DEBUG_LOG("accept client[%d]->io.fd=%d", client - g_handle->all_client, s);
		ev_io_init(&(client->io), read_connection, s, EV_READ);
		ev_io_start(EV_A_ &(client->io));
	} while ( s > -1);
}

int ty_pending_fetch_item(Pending_handle_t *_this, int &index, int &sock)
{
	Client_item_t *client = client_work_ghead(_this);
	sock = client->io.fd;
	index = client - _this->all_client;
	WARNING_LOG("fetch client[%d]->io.fd=%d", index, sock);
	return 0;
}

void ty_pending_reset_item(Pending_handle_t *_this, int index, bool keep_alive)
{
	if (index < 0 || index >= _this->max_client_num) {
		WARNING_LOG("index[%d] not in [0, %d)", index, _this->max_client_num);
		return;
	}
	Client_item_t *client = _this->all_client + index;
	if (keep_alive) {
		DEBUG_LOG("keep alive client[%d]->io.fd=%d", index, client->io.fd);
		client->status = AS_CLIENT_IDLE;
		return;
	}
	ev_io_stop(_this->loop, &(client->io));
	as_lingering_close( client->io.fd );
	WARNING_LOG("free client[%d]", index);
	client->io.fd = -1;
	client_idle_push(client);
}

int ty_pending_listen(Pending_handle_t *_this, const int fd)
{
	setnonblock(fd);
	ev_io_init(&(_this->listen_w), accept_connection, fd, EV_READ);
	ev_io_start(_this->loop, &(_this->listen_w));
	return 0;
}

int ty_pending_listen_port(Pending_handle_t *_this, const int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if ( fd == -1 ) {
		WARNING_LOG("socket fail [%d][%s]", errno, strerror(errno));
		return -1;
	}

	int optval = -1;
	int int_len = sizeof(int);
	if (getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t*)&int_len) == -1) {
		WARNING_LOG("Error when getting socket option SO_REUSEADDR\n");
		optval = 0;
	}
	if (optval == 0) {
		optval = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	}

	int_len = sizeof(int);
	if (getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, (socklen_t*)&int_len) == -1) {
		WARNING_LOG("Error when getting socket option SO_KEEPALIVE\n");
		optval = 0;
	}
	if (optval == 0) {
		optval = 1;
		setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
	}

	struct sockaddr_in al;
	memset(&al, 0, sizeof(al));

	al.sin_family = AF_INET;
	al.sin_addr.s_addr = INADDR_ANY;
	al.sin_port = htons( port );

	if (bind(fd, (struct sockaddr *)&al, sizeof(al)) < 0) {
		WARNING_LOG("failed to bind [%d][%s]", errno, strerror(errno));
		return -1;
	}

	if (listen(fd, 128) < 0) {
		WARNING_LOG("failed to listen to socket [%d][%s]", errno, strerror(errno));
		return -1;
	}

	return ty_pending_listen(_this, fd);
}

void ty_pending_run(Pending_handle_t *_this)
{
	ev_run (_this->loop, 0);
}

int ty_pending_del(Pending_handle_t *_this)
{
	if (IS_NULL( _this ) || IS_NULL( g_handle )) {
		return -1;
	}
	ev_break (_this->loop, EVBREAK_ALL);
	pthread_mutex_destroy(&(_this->m_idle_mutex));
	pthread_mutex_destroy(&(_this->m_mutex));
	pthread_cond_destroy(&(_this->m_cond));
	free(_this);
	g_handle = NULL;
	return 0;
}

int as_socket_send(int fd, const void *buf, int len)
{
	int rv;
	int nwrite = len;
	int wlen = 0;
	const char *pbuf = (const char *)buf;

	do {
		do {
			rv = write(fd, pbuf + wlen, nwrite);
			//DEBUG_LOG("w->%d %d %d [%d:%s]", fd, nwrite, rv, errno, strerror(errno));
		} while (rv == -1 && ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK ));
		if (errno == EPIPE) {
			return -1;
		}
		if (rv == -1) {
			return wlen;
		}
		nwrite -= rv;
		wlen += rv;
	} while(nwrite > 0);

	return wlen;
}

int as_socket_recv(int fd, void *buf, int len)
{
	int rv;
	int nread = len;
	int rlen = 0;
	char *pbuf = (char *)buf;

	do {
		do {
			rv = read(fd, pbuf + rlen, nread);
			//DEBUG_LOG("r->%d %d %d [%d:%s]", fd, nread, rv, errno, strerror(errno));
		} while (rv == -1 && ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
		if (errno == EPIPE) {
			return -1;
		}
		if (rv <= 0) {
			return rlen;
		}
		nread -= rv;
		rlen += rv;
	} while (nread > 0);
	return rlen;
}

Pending_handle_t *ty_pending_creat(const int max_client_num)
{
	if (!IS_NULL( g_handle )) {
		return NULL;
	}
	int mnum = 0;
	if (max_client_num < 1) {
		mnum = 1024;
	} else {
		mnum = max_client_num;
	}

	Pending_handle_t *_new = (Pending_handle_t *) malloc ( sizeof(Pending_handle_t) + sizeof(Client_item_t) * mnum );
	if (IS_NULL( _new )) {
		return NULL;
	}
	memset(_new, 0, sizeof(Pending_handle_t));
	_new->max_client_num = mnum;

	for (int i = mnum; i--;) {
		_new->all_client[i].next = _new->idle;
		_new->idle = _new->all_client + i;
	}
	pthread_mutex_init(&(_new->m_idle_mutex), NULL);
	pthread_mutex_init(&(_new->m_mutex), NULL);
	pthread_cond_init(&(_new->m_cond), NULL);
	_new->loop = ev_default_loop (EVBACKEND_EPOLL /* EVBACKEND_SELECT */);
	if (IS_NULL( _new->loop )) {
		free(_new);
		return NULL;
	}

	g_handle = _new;

	return _new;
}

