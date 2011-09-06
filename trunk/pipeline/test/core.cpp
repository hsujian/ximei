#include "core.h"

extern Pipeline_t *g_pending_handle;


void unexpectedsignal(int sig, siginfo_t * info, void *context)
{
	signal(sig, SIG_IGN);	/* ignore furture signals */
	ty_writelog(TY_LOG_FATAL,
			"[%ld] Unexpected signal %d caught.source pid is %d.code=%d.",
			pthread_self(), sig, info->si_pid, info->si_code);

	//do something
	/*

*/
	if (sig != 15 && sig != 2)
		abort();
	else
		exit(2);
}


void signalsetup()
{
	struct sigaction act;
	act.sa_flags = SA_SIGINFO;

	//ignored signals
	act.sa_sigaction = NULL;
	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGCLD, SIG_IGN);
#ifdef	SIGTSTP
	signal(SIGTSTP, SIG_IGN);	/* background tty read */
#endif

#ifdef	SIGTTIN
	signal(SIGTTIN, SIG_IGN);	/* background tty read */
#endif

#ifdef	SIGTTOU
	signal(SIGTTOU, SIG_IGN);	/* background tty write */
#endif

	//unexpected signal
	act.sa_sigaction = unexpectedsignal;
	sigaction(SIGILL, &act, NULL);
	//sigaction ( SIGINT, &act,NULL);
	sigaction(SIGQUIT, &act, NULL);
#ifdef SIGEMT
	sigaction(SIGEMT, &act, NULL);
#endif
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGSYS, &act, NULL);
	sigaction(SIGPWR, &act, NULL);
	//sigaction ( SIGTERM, &act,NULL);
#ifdef SIGSTKFLT
	sigaction(SIGSTKFLT, &act, NULL);
#endif
	//
	return;
}


void *service_thread(void *pti)
{
	{
		Thread_info_t *pthread_info = (Thread_info_t *)pti;
		char thrname[64];
		snprintf(thrname, 64, "service_thread.%d", pthread_info->thrd_no);
		ty_log_open_r(thrname, NULL);
	}

	int sock = 0;
	int s_index;
	int rlen = 0;
	int ret = 0;
	int q = 0;
	int qlen = 0;
	char *p = NULL;

	int max_read_size = 1024*1024;
	char *read_buf = (char *)malloc(max_read_size);
	if (read_buf == NULL) {
		ty_writelog(TY_LOG_FATAL, "malloc for read_buf failed");
		return NULL;
	}

	Log_info_t loginfo;

	struct timeval tvstart, tvend, tv1, tv2;
	u_int timeused;

	while (1) {
		memset(&loginfo, 0, sizeof(Log_info_t));
		loginfo.status = OK;
		pipeline_fetch_item(g_pending_handle, &s_index, &sock);
		GetTimeCurrent(tvstart);

		ret = socket_recv(sock, read_buf, max_read_size);
		if (ret < 0) {
			loginfo.status = UI_GET_ERROR;
			goto message_end;
		}
		if (ret == 0 && errno == EAGAIN) {
			loginfo.status = NO_REQUEST;
			goto message_end;
		}
		rlen = ret;
		DEBUG_LOG("recv %d errno=%d", rlen, errno);

		q = 0;
		qlen = 0;
		p = strstr(read_buf, "q=");
		if (p) {
			q = atoi(p+2);
			qlen = sprintf(read_buf, "%d", q);
		}
		rlen = snprintf(read_buf, max_read_size, ""
				"HTTP/1.0 200 OK\r\n"
				"Content-Length: %d\r\n"
				"Content-Type: text/plain\r\n"
				"\r\n%d", qlen, q);
		// do some other things

send_to_UI:
		ret = socket_send(sock, read_buf, rlen);
		if (ret < 0)
			loginfo.status = UI_PUT_ERROR;
		GetTimeCurrent(tvend);
		SetTimeUsed(timeused, tvstart, tvend);
message_end:					

		ty_writelog(TY_LOG_NOTICE, "[status %d][t %u]", loginfo.status, timeused);
		if (loginfo.status == UI_GET_ERROR || loginfo.status == UI_PUT_ERROR) {
			pipeline_reset_item(g_pending_handle, s_index, 0);
			ty_writelog(TY_LOG_WARNING, "ui connect broken");
		} else {
			pipeline_reset_item(g_pending_handle, s_index, 1);
		}
	}
}

