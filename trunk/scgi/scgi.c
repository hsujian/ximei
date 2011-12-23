#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "scgi.h"
#include "net.h"

#define DEBUG(fmt, arg...) printf("<%s(%s:%d)> " fmt, __FUNCTION__, __FILE__, __LINE__, ##arg)

void
scgi_init_request(scgi_t *scgi)
{
	memset(scgi, 0, sizeof(scgi_t));
}

void
scgi_request(scgi_t *scgi, int socket)
{
	scgi->socket = socket;
	scgi->raw_header.elts = 0;
	scgi->in_headers.elts = 0;
	scgi->out_headers.elts = 0;
}

static int
header_array_add(header_array_t *hat, const char *name, const char *val)
{
	if (hat->elts >= hat->size) {
		header_t *h = (header_t *)realloc(hat->array, sizeof(header_t)*(hat->size+5));
		if (h == NULL) {
			return -1;
		}
		hat->array = h;
		hat->size += 5;
	}
	int i = hat->elts;
	++ hat->elts;
	hat->array[i].name = name;
	hat->array[i].value = val;
	return 0;
}

int
scgi_set_header(scgi_t *scgi, const char *name, const char *val)
{
	return header_array_add(& scgi->out_headers, name, val);
}

int
scgi_get_request(scgi_t *scgi)
{
	int rv = 0;
	unsigned long len, i;
	char *p;
	int j;

	/* Decode header netstring */
	char buf[16];
	len = 0;
	i = 0;
	do {
		rv = recv(scgi->socket, buf, sizeof(buf), MSG_PEEK);
		if (rv == 0) {
			DEBUG("recv MSG_PEEK 0\n");
			return -1;
		}
		if (rv == -1) {
			DEBUG("recv MSG_PEEK -1: %d %s\n", errno, strerror(errno));
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			}
			return -1;
		} else {

			if (buf[0] < '0' || buf[0] > '9') {
				DEBUG("recv not number\n");
				return -1;
			}

			scgi->raw_header.elts = atoi(buf);
			DEBUG("recv number:%d\n", scgi->raw_header.elts);
			int i = 0;
			do {
				if (buf[i] < '0' || buf[i] > '9') {
					break;
				}
				i++;
			} while(1);

			if (buf[i] == ':') {
				i++;
				DEBUG("recv skip %d char\n", i);
				do {
					rv = recv(scgi->socket, buf, i, 0);
				} while(rv == -1 && errno == EINTR);
				DEBUG("skip done\n");
				break;
			} else {
				DEBUG("maybe format error, should be ':'\n");
				return -1;
			}
		}

	} while(rv == -1 && errno == EINTR);

	if (scgi->raw_header.elts < 0) {
		scgi->raw_header.elts = 0;
	}

	if (scgi->raw_header.elts + 1 > scgi->raw_header.size) {
		char *new_buf = (char *)realloc(scgi->raw_header.array, scgi->raw_header.elts + 2);
		if (new_buf == NULL) {
			return -1;
		}
		scgi->raw_header.array = new_buf;
		scgi->raw_header.size = malloc_usable_size(new_buf);
	}

	len = socket_recv(scgi->socket, scgi->raw_header.array, scgi->raw_header.elts + 1);
	if (scgi->raw_header.elts + 1 != len) {
		return -1;
	}

	if (scgi->raw_header.array[scgi->raw_header.elts] != ',') {
		DEBUG("maybe format error, should be ','\n");
		return -1;
	}
	scgi->raw_header.array[scgi->raw_header.elts] = '\0';

	len = scgi->raw_header.elts;
	/* Now put header/value pairs into array */
	for (i = 0, p = scgi->raw_header.array, j = 0; i < len;) {
		const char *name, *val;

		name = p;
		while (i++ < len && *(p++));
		val = p;
		while (i++ < len && *(p++));

		header_array_add(& scgi->in_headers, name, val);
		DEBUG("header: %s:%s\n", name, val);
	}

	return 1;
}

int
scgi_send_response(scgi_t *scgi, void *buf, int len)
{
	char head[128];
	int headlen = sprintf(head, "Status: 200 OK\r\nContent-length: %d\r\nContent-Type: text/plain\r\n\r\n", len);
	int rv = socket_send_all(scgi->socket, head, headlen);
	if (rv == -1) {
		return -1;
	}
	return socket_send_all(scgi->socket, buf, len);
}

