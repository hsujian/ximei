#include "ty_log.h"
#include "net.h"

#include<signal.h>
#include <stdio.h>
#include <string.h>

int main()
{
	signal(SIGPIPE, SIG_IGN);
	char buf[8192];
	int ret = 0;
	int sock = socket_tcpconnect4("127.0.0.1", 1234);
	if (sock < 0) {
		DEBUG_LOG("tcpconn [%d] errno=%d\n", sock, errno);
		return -1;
	}
	//setnonblock(sock);
	strcpy(buf, "client");
	while(1) {
		ret = wait_for_io_or_timeout(sock, 0, -1);
		if (ret != 1) {
			DEBUG_LOG("wait [%d] ret=%d errno=%d\n", sock, ret, errno);
			continue;
		}
		ret = socket_send(sock, buf, strlen(buf));
		if (ret < 0) {
			DEBUG_LOG("close:errno[%d] ret[%d/%d] msg[%s]\n", errno, ret, strlen(buf), buf);
			lingering_close(sock);
			break;
		}
//		ret = socket_send(sock, buf, 0);
		DEBUG_LOG("send:errno[%d] ret[%d/%d] msg[%s]\n", errno, ret, strlen(buf), buf);
		sleep(1);
	}
	sync();
	return 0;
}

