#include "net.h"
#include "scgi.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <error.h>

#define DEBUG(fmt, arg...) printf("<%s(%s:%d)> " fmt, __FUNCTION__, __FILE__, __LINE__, ##arg)

int main(int argc, char **argv)
{
	scgi_t scgi;

	scgi_init_request(&scgi);

	char buf[8192];
	int ret = 0;
	int fd = 0;
	int sock = socket_tcplisten_port(1025);
	DEBUG("listen: 1025 fd=%d errno=%d\n", sock, errno);
	setnonblock(sock);
	while(1) {
		ret = wait_for_io_or_timeout(sock, 1, -1);
		if (ret != 1) {
			DEBUG("wait [%d] ret=%d errno=%d\n", sock, ret, errno);
			continue;
		}
		fd = accept(sock, NULL, NULL);
		if (fd == -1) {
			DEBUG("accept [%d] errno=%d\n", fd, errno);
			continue;
		}
		//setnonblock(fd);
		scgi_request(&scgi, fd);
		while(1) {
			/*
			ret = wait_for_io_or_timeout(fd, 1, -1);
			if (ret != 1) {
				DEBUG("wait [%d] ret=%d errno=%d\n", fd, ret, errno);
				continue;
			}
			*/
			DEBUG("pre recv\n");
			
			ret = scgi_get_request(&scgi);
			if (ret < 0) {
				lingering_close(fd);
				break;
			}
			if (ret == 1) {
				DEBUG("scgi_get 1\n");
				int len = snprintf(buf, sizeof(buf), "hello world");
				buf[len] = '\0';
				buf[len+1] = '\0';
				ret = scgi_send_response(&scgi, buf, len+1);
				if (ret != len+1) {
					lingering_close(fd);
					break;
				}
				DEBUG("scgi_send_response done %d=%d\n", ret, len);
			}
		}
	}
	return 0;
}

