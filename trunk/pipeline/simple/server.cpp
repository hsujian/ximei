#include "ty_log.h"
#include "net.h"

#include <stdio.h>
#include <string.h>

int main()
{
	char buf[8192];
	int ret = 0;
	int fd = 0;
	int sock = socket_tcplisten_port(1234);
	DEBUG_LOG("listen: 1234 fd=%d errno=%d\n", sock, errno);
	setnonblock(sock);
	while(1) {
		ret = wait_for_io_or_timeout(sock, 1, -1);
		if (ret != 1) {
			DEBUG_LOG("wait [%d] ret=%d errno=%d\n", sock, ret, errno);
			continue;
		}
		fd = accept(sock, NULL, NULL);
		if (fd == -1) {
			DEBUG_LOG("accept [%d] errno=%d\n", fd, errno);
			continue;
		}
		setnonblock(fd);
		while(1) {
			ret = wait_for_io_or_timeout(fd, 1, -1);
			if (ret != 1) {
				DEBUG_LOG("wait [%d] ret=%d errno=%d\n", fd, ret, errno);
				continue;
			}
			DEBUG_LOG("pre recv\n");
			ret = socket_recv(fd, buf, 8192);
			DEBUG_LOG("recv:errno[%d] ret[%d/8192] msg[%s]\n", errno, ret, buf);
			if (ret < 0) {
				lingering_close(fd);
				break;
			}
		}
	}
	return 0;
}

