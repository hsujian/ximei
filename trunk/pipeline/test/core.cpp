#include "core.h"

extern greeting_bonze_t *g_pending_handle;


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
		snprintf(thrname, 64, "st.%d", pthread_info->thrd_no);
		ty_log_open_r(thrname, NULL);
	}

	int sock = 0;
	int rlen = 0;
	int ret = 0;
	int q = 0;
	int qlen = 0;
	char *p = NULL;

	const char *request = NULL;
	char *response = NULL;
	Log_info_t loginfo;

	struct timeval tvstart, tvend;
	u_int timeused;

	while (1) {
		memset(&loginfo, 0, sizeof(Log_info_t));
		loginfo.status = OK;
		greeting_bonze_deal(g_pending_handle, &sock, (const char **)&request, &rlen, (const char **)&response);
		GetTimeCurrent(tvstart);
		DEBUG_LOG("recv %d [%s]", rlen, request);

		q = 0;
		qlen = 0;
		p = strstr(request, "q=");
		if (p) {
			q = atoi(p+2);
			qlen = sprintf(response, "%d", q);
		}
		rlen = snprintf(response, MAX_RESPONSE_LEN, ""
				"HTTP/1.0 200 OK\r\n"
				"Content-Length: %d\r\n"
				"Content-Type: text/plain\r\n"
				"\r\n%d", qlen, q);
		// do some other things

send_to_UI:
		greeting_bonze_send_off(g_pending_handle, sock, rlen);
		GetTimeCurrent(tvend);
		SetTimeUsed(timeused, tvstart, tvend);
message_end:					

		ty_writelog(TY_LOG_NOTICE, "[status %d][t %u]", loginfo.status, timeused);
	}
}

