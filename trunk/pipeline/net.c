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
		WARNING_LOG("socket fail [%d]", errno);
		return -1;
	}

	int optval = -1;
	int int_len = sizeof(int);
	if (getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, (socklen_t*)&int_len) == -1) {
		WARNING_LOG("Error when getting socket option SO_REUSEADDR");
		optval = 0;
	}
	if (optval == 0) {
		optval = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	}

	int_len = sizeof(int);
	if (getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, (socklen_t*)&int_len) == -1) {
		WARNING_LOG("Error when getting socket option SO_KEEPALIVE");
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
		WARNING_LOG("failed to bind [%d]", errno);
		close(fd);
		return -1;
	}

	if (listen(fd, 128) < 0) {
		WARNING_LOG("failed to listen to socket [%d]", errno);
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
				errno = EPIPE;
				return -1;
			default:
				nread -= rv;
				rlen += rv;
				break;
		}
	} while (nread > 0);
	return rlen;
}

int socket_tcpconnect4(const char *ip, const int port)
{
	int s;
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return -1;
	}
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

int wait_for_io_or_timeout(int socket, int for_read, int timeout_ms)
{
	struct pollfd pfd;
	int rc;

	pfd.fd     = socket;
	pfd.events = for_read ? POLLIN : POLLOUT;

	do {
		rc = poll(&pfd, 1, timeout_ms);
	} while (rc == -1 && errno == EINTR);
	return rc;
}

