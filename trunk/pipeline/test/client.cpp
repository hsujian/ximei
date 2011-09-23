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
	int sock = socket_tcpconnect4("127.0.0.1", 8080);
	if (sock < 0) {
		DEBUG_LOG("tcpconn [%d] errno=%d\n", sock, errno);
		return -1;
	}
	//setnonblock(sock);
	while(1) {
		ret = wait_for_io_or_timeout(sock, 0, -1);
		if (ret != 1) {
			DEBUG_LOG("wait [%d] ret=%d errno=%d\n", sock, ret, errno);
			continue;
		}
		strcpy(buf, "client q=1");
		ret = socket_send(sock, buf, 128 /*strlen(buf)*/);
		if (ret < 0) {
			DEBUG_LOG("close:errno[%d] ret[%d/%d] msg[%s]\n", errno, ret, strlen(buf), buf);
			lingering_close(sock);
			break;
		}
		DEBUG_LOG("send:errno[%d] ret[%d/%d] msg[%s]\n", errno, ret, strlen(buf), buf);
		ret = socket_recv(sock, buf, 65);
		DEBUG_LOG("recv:errno[%d] ret[%d/%d] msg[%s]\n", errno, ret, strlen(buf), buf);
		usleep(1);
	}
	sync();
	return 0;
}

