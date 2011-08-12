#ifndef __TY_PENDING_POOL_H_
#define __TY_PENDING_POOL_H_

#include <ev.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct Pending_handle_t Pending_handle_t;

extern Pending_handle_t *ty_pending_creat(const int max_client_num = 0);

extern int ty_pending_del(Pending_handle_t *phandle);

extern int ty_pending_fetch_item(Pending_handle_t *phandle,int &index, int &sock);

extern void ty_pending_reset_item(Pending_handle_t *phandle, int index, bool keep_alive);

extern void ty_pending_run(Pending_handle_t *phandle);

extern int ty_pending_listen(Pending_handle_t *_this, const int fd);
extern int ty_pending_listen_port(Pending_handle_t *_this, const int port);

extern int as_socket_send(int fd, const void *buf, int len);

extern int as_socket_recv(int fd, void *buf, int len);

#endif
