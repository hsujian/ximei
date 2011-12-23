#ifndef XDJ_SCGI_LIB_INCLUDE_H_
#define XDJ_SCGI_LIB_INCLUDE_H_

typedef struct {
	const char *name;
	const char *value;
} header_t;

typedef struct {
	int size;
	int elts;
	header_t *array;
} header_array_t;

typedef struct {
	int size;
	int elts;
	char *array;
} char_array_t;

typedef struct {
	int socket;

	char_array_t raw_header;
	header_array_t in_headers;
	header_array_t out_headers;
} scgi_t;


void
scgi_init_request(scgi_t *scgi);

void
scgi_request(scgi_t *scgi, int socket);

int
scgi_get_request(scgi_t *scgi);

int
scgi_send_response(scgi_t *scgi, void *buf, int len);

#endif /* define XDJ_SCGI_LIB_INCLUDE_H_ */
