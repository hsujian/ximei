#ifndef __GREETING_BONZE_H_
#define __GREETING_BONZE_H_

#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct greeting_bonze_t greeting_bonze_t;

typedef void (*guest_deal_fn) (greeting_bonze_t *gb, const int fd);

greeting_bonze_t *greeting_bonze_new(const int capacity, const int in_size, const int out_size);

void greeting_bonze_del(greeting_bonze_t *gb);

int greeting_bonze_deal(greeting_bonze_t *gb, int *fd, const char **pbuf, int *rlen, const char **out_buf);

int greeting_bonze_send_off(greeting_bonze_t *gb, int fd, int out_len);

int greeting_bonze_do(greeting_bonze_t *gb);

int greeting_bonze_listen(greeting_bonze_t *gb, const int fd);
int greeting_bonze_listen_port(greeting_bonze_t *gb, const int port);

int greeting_bonze_get_in_len(greeting_bonze_t *gb, int fd);
int greeting_bonze_get_out_len(greeting_bonze_t *gb, int fd);
void greeting_bonze_set_in_len(greeting_bonze_t *gb, int fd, int len);
void greeting_bonze_set_out_len(greeting_bonze_t *gb, int fd, int len);
char *greeting_bonze_get_in_buf(greeting_bonze_t *gb, int fd);
char *greeting_bonze_get_out_buf(greeting_bonze_t *gb, int fd);

void greeting_bonze_set_guest_fn(greeting_bonze_t *gb, guest_deal_fn fn);

#ifdef __cplusplus
}
#endif

#endif /* define GREETING_BONZE */
