#ifndef __PIPELINE_SOCKET_H_
#define __PIPELINE_SOCKET_H_

#include <sys/uio.h>
//#include "ty_log.h"
#ifndef __TY_LOG_H_

#undef DEBUG_LOG
#undef WARNING_LOG
#define DEBUG_LOG(fmt, arg...)
#define WARNING_LOG(fmt, arg...)

#endif

#ifdef __cplusplus
extern "C" {
#endif

int setnonblock(int fd);
int lingering_close(int fd);
int socket_listen_port(const int port);

int socket_sendv(int fd, struct iovec *vec, int nvec);
int socket_send(int fd, const void *buf, int len);

int socket_recv(int fd, void *buf, int len);

int socket_connect(const char *ip, const int port, int domain, int type, int protocol);
int socket_tcpconnect4(const char *ip, const int port);
int socket_connect_unix(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* __PIPELINE_SOCKET_H_ */

