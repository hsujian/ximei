#ifndef __PIPELINE_SOCKET_H_
#define __PIPELINE_SOCKET_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

int setnonblock(int fd);
int lingering_close(int fd);
int socket_tcplisten_port(const int port);

int wait_for_io_or_timeout(int socket, int for_read, int timeout_ms);

int socket_sendv(int fd, struct iovec *vec, int nvec);
int socket_send(int fd, const void *buf, int len);
int socket_send_all(int fd, const void *buf, int len);

int socket_recv(int fd, void *buf, int len);

int socket_tcpconnect4(const char *ip, const int port);
int socket_connect_unix(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* __PIPELINE_SOCKET_H_ */

