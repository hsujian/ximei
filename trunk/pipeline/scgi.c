#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "net.h"

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
header_array_add(const header_array_t *hat, const char *name, const char *val)
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
	return header_array_add(scgi->out_headers, name, val);
}

int
scgi_get_request(scgi_t *scgi)
{
	int rv = 0;
	int error = SCGI_ERROR;
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
			return -1;
		}
		if (rv == -1) {
			if (error == EAGAIN || error == EINTR) {
				continue;
			}
			return -1;
		}

		if (buf[0] < '0' || buf[0] > '9') {
			return -1;
		}

		scgi->raw_header.elts = atoi(buf);
		int i = 0;
		do {
			if (buf[i] < '0' || buf[i] > '9') {
				break;
			}
			i++;
		} while(1);

		if (buf[i] == ':') {
			i++;
			do {
				rv = recv(scgi->socket, buf, i, 0);
			} while(rv == -1 && errno == EINTR);
			break;
		} else {
			return -1;
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

		header_array_add(scgi->in_headers, name, val);
	}

	return 1;
}

int
scgi_send_response(scgi_t *scgi, void *buf, int len)
{
	const char *head = "Status: 200 OK\r\nContent-Type: text/plain\r\n\r\n";
	int headlen = strlen(head);
	int rv = socket_send_all(scgi->socket, head, headlen);
	if (rv == -1) {
		return -1;
	}
	return socket_send_all(scgi->socket, buf, len);
}

