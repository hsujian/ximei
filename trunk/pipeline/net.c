#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <netinet/in.h>

#include "net.h"

int setnonblock(int fd)
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

int lingering_close( int fd )
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
		DEBUG_LOG("lc %d rv %d[%d]", fd, rv, errno);
	} while (rv > 0);
	return close(fd);
}

int socket_listen_port(const int port)
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
		close(fd);
		return -1;
	}

	if (listen(fd, 128) < 0) {
		WARNING_LOG("failed to listen to socket [%d][%s]", errno, strerror(errno));
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
		} while (rv == -1 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));

		if (rv == -1) {
			return -1;
		}
		bytes += rv;
		/* recalculate vec to deal with partial writes */
		while (rv > 0) {
			if (rv < vec[i].iov_len) {
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
			//DEBUG_LOG("w->%d %d %d [%d:%s]", fd, nwrite, rv, errno, strerror(errno));
		} while (rv == -1 && ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
		//if (errno == EPIPE) { return -1; }
		if (rv == -1) {
			return -1;
		}
		nwrite -= rv;
		wlen += rv;
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
			//DEBUG_LOG("r->%d %d %d [%d:%s]", fd, nread, rv, errno, strerror(errno));
		} while (rv == -1 && ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK));
		if (rv == 0) {
			return rlen;
		}
		//if (errno == EPIPE) { return -1; }
		if (rv == -1) {
			return -1;
		}
		nread -= rv;
		rlen += rv;
	} while (nread > 0);
	return rlen;
}

int socket_connect(const char *ip, const int port, int domain, int type, int protocol)
{
	int s;
	if ((s = socket(domain, type, protocol)) < 0) {
		return -1;
	}
	if (domain == AF_INET) {
		struct sockaddr_in addr;
		bzero(&addr,sizeof(addr));
		addr.sin_family = domain;
		addr.sin_port = htons( port );
		addr.sin_addr.s_addr = inet_addr(ip);
		return connect(s, (struct sockaddr *)&addr, sizeof(addr));
	}
	if (domain == AF_UNIX) {
		struct sockaddr_un addr;
		bzero(&addr, sizeof(addr));
		addr.sun_family = domain;
		snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", ip);
		return connect(s, (struct sockaddr *)&addr, sizeof(addr));
	}
	return -1;
}

int socket_tcpconnect4(const char *ip, const int port)
{
	return socket_connect(ip, port, AF_INET, SOCK_STREAM, 0);
}

int socket_connect_unix(const char *path)
{
	return socket_connect(path, 0, AF_UNIX, SOCK_STREAM, 0);
}

