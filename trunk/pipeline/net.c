#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <poll.h>

#include <time.h>

#include "net.h"

#ifndef NDEBUG

#ifndef DEBUG_LOG
#define DEBUG_LOG(fmt, arg...) printf("<%s(%s:%d)> " fmt, __FUNCTION__, __FILE__, __LINE__, ##arg)
#endif

#else

#define DEBUG_LOG(fmt, arg...)

#endif

int setnonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		return fcntl(fd, F_SETFL, O_NONBLOCK);
	}
	if (!(flags & O_NONBLOCK)) {
		flags |= O_NONBLOCK;
		return fcntl(fd, F_SETFL, flags);
	}
	return 0;
}

int lingering_close( int fd )
{
	char buf[512];
	int rv;
	shutdown(fd, SHUT_WR);
	setnonblock(fd);
	time_t tv = time(NULL) + 1;
	do {
		rv = read(fd, buf, 512);
		if (time(NULL) > tv) {
			break;
		}
	} while (rv == -1 && errno == EINTR);
	return close(fd);
}

int socket_tcplisten_port(const int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if ( fd == -1 ) {
		return -1;
	}

	int optval = -1;
	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

	optval = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

	struct sockaddr_in al;
	memset(&al, 0, sizeof(al));

	al.sin_family = AF_INET;
	al.sin_addr.s_addr = INADDR_ANY;
	al.sin_port = htons( port );

	if (bind(fd, (struct sockaddr *)&al, sizeof(al)) < 0) {
		close(fd);
		return -1;
	}

	if (listen(fd, 128) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

int socket_domain_listen(const char *path)
{
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if ( fd == -1 ) {
		return -1;
	}

	int optval = -1;

	optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

	optval = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

	struct sockaddr_un al;
	memset(&al, 0, sizeof(al));

	al.sun_family = AF_UNIX;
	snprintf(al.sun_path, sizeof(al.sun_path), "%s", path);
	unlink(al.sun_path);
	int len = strlen(al.sun_path) + sizeof(al.sun_family);

	if (bind(fd, (struct sockaddr *)&al, len) < 0) {
		close(fd);
		return -1;
	}

	if (listen(fd, 128) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

int socket_sendv(int fd, struct iovec *vec, int nvec)
{
	int i = 0, bytes = 0, rv = 0;

	while (i < nvec) {
		do {
			rv = writev(fd, &vec[i], nvec - i);
		} while (rv == -1 && (errno == EINTR || errno == EAGAIN));

		if (rv == -1) {
			return -1;
		}
		bytes += rv;
		/* recalculate vec to deal with partial writes */
		while (rv > 0) {
			if (rv < (int)vec[i].iov_len) {
				vec[i].iov_base = (char *) vec[i].iov_base + rv;
				vec[i].iov_len -= rv;
				rv = 0;
			} else {
				rv -= vec[i].iov_len;
				++i;
			}
		}
	}

	/* We should get here only after we write out everything */
	return bytes;
}

int socket_send(int fd, const void *buf, int len)
{
	int rv;
	int nwrite = len;
	int wlen = 0;
	const char *pbuf = (const char *)buf;

	do {
		do {
			rv = write(fd, pbuf + wlen, nwrite);
		} while (rv == -1 && errno == EINTR);
		switch(rv) {
			case -1:
				if (errno == EAGAIN) {
					return wlen;
				}
				return -1;
				break;
			case 0:
				return wlen;
				break;
			default:
				nwrite -= rv;
				wlen += rv;
				break;
		}
	} while(nwrite > 0);

	return wlen;
}

int socket_recv(int fd, void *buf, int len)
{
	int rv;
	int nread = len;
	int rlen = 0;
	char *pbuf = (char *)buf;

	do {
		do {
			rv = read(fd, pbuf + rlen, nread);
		} while (rv == -1 && errno == EINTR);
		switch (rv) {
			case -1:
				if (errno == EAGAIN) {
					return rlen;
				}
				return -1;
				break;
			case 0:
				return rlen;
			default:
				nread -= rv;
				rlen += rv;
				break;
		}
	} while (nread > 0);
	return rlen;
}

int socket_send_all(int fd, const void *buf, int len)
{
	int l = len, rv;
	const char *p = (const char *)buf;
	do {
		rv = socket_send(fd, p, l);
		if (rv == -1 && errno != EAGAIN) {
			return -1;
		}
		p += rv;
		l -= rv;
	} while (l>0);
	return len;
}

int socket_tcpconnect4(const char *ip, const int port)
{
	int s;
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return -1;
	}
	int optval = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

	struct sockaddr_in addr;
	bzero(&addr, sizeof(struct sockaddr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons( port );
	addr.sin_addr.s_addr = inet_addr(ip);
	if ( connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0 ) {
		return -1;
	}
	return s;
}

int socket_connect_unix(const char *path)
{
	int s;
	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		return -1;
	}
	int optval = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));

	struct sockaddr_un addr;
	bzero(&addr, sizeof(struct sockaddr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));
	addr.sun_path[ sizeof(addr.sun_path) - 1 ] = '\0';
	if ( connect(s, (struct sockaddr *)&addr, SUN_LEN(&addr)) < 0 ) {
		return -1;
	}
	return s;
}

int is_socket_need_close(int socket)
{
	struct tcp_info info;
	int len = sizeof(info);
	getsockopt(socket, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len);
	return info.tcpi_state == TCP_CLOSE_WAIT;
}

int wait_for_io(int socket, int for_read, int timeout_ms, int *revents)
{
	struct pollfd pfd;
	int rv;

	pfd.fd     = socket;
	pfd.events = for_read ? POLLIN : POLLOUT;
	pfd.events |= POLLRDHUP;
	pfd.revents = 0;

	*revents = 0;

	do {
		rv = poll(&pfd, 1, timeout_ms);
	} while (rv == -1 && errno == EINTR);

	//printf("revents=%#x\n", pfd.revents);
	if (rv == 1) {
		if ((pfd.revents & POLLRDHUP) || (pfd.revents & POLLHUP) || (pfd.revents & POLLERR)) {
			*revents = pfd.revents;
		}
	}
	return rv;
}

int wait_for_io_or_timeout(int socket, int for_read, int timeout_ms)
{
	int epipe = 0;
	int rv = wait_for_io(socket, for_read, timeout_ms, &epipe);
	if (rv == 1 && epipe) {
		return -1;
	}
	return rv;
}

