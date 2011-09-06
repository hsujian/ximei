#include "core.h"

Conf_t g_conf;
char g_sys_path[MAX_PATH_LEN];
int g_listen_sock;
Pending_handle_t *g_pending_handle;


void usage(char * prname)
{
	printf("\n");
	printf("Project    : %s\n", PROJECT_NAME);
	printf("Version    : %s\n", SERVER_VERSION);
	printf("Usage: %s [-v]\n", prname);
	printf("	[-v]  show version info\n");
	printf("\n");
}

int main_parse_option(int argc, char **argv)
{
	if(argc != 1)
	{
		usage(argv[0]);
		exit(1);
	}

	char *mainpath = getenv("SERVER_PATH");
	if (mainpath == NULL)
		snprintf(g_sys_path, sizeof(g_sys_path), "../");
	else
		snprintf(g_sys_path, sizeof(g_sys_path), "%s", mainpath);
	
	return 1;
}

int load_conf()
{
	char conf_path[MAX_PATH_LEN];
	Tyconf_t sconf;
	snprintf(conf_path, sizeof(conf_path), "%s/conf", g_sys_path);
	if (ty_readconf(conf_path, "server.conf", &sconf) < 0)
	{
		ty_writelog(TY_LOG_FATAL, "<load_conf> read conf [%s/server.conf] failed!", conf_path);
		return -1;
	}
	if (!ty_getconfint(&sconf, "Listen_port", &g_conf.listen_port))
	{
		ty_writelog(TY_LOG_FATAL, "<load_conf> can not get listen_port");
		return -1;
	}

	if (!ty_getconfint(&sconf, "Read_timeout", &g_conf.read_tmout))
	{
		ty_writelog(TY_LOG_WARNING, "<load_conf> can not get Read_timeout");
		g_conf.read_tmout = -1;
	}

	if (!ty_getconfint(&sconf, "Write_timeout", &g_conf.write_tmout))
	{
		ty_writelog(TY_LOG_WARNING, "<load_conf> can not get Write_timeout");
		g_conf.write_tmout = -1;
	}

	if (!ty_getconfint(&sconf, "Thread_num", &g_conf.thread_count))
	{
		ty_writelog(TY_LOG_WARNING, "<load_conf> can not load thread_num");
		g_conf.thread_count = DEFAULT_THREAD_NUM;
	}


	if (!ty_getconfstr(&sconf, "Log_path", g_conf.log_path))
	{
		ty_writelog(TY_LOG_WARNING, "<load_conf> can not load log_path");
		snprintf(g_conf.log_path, sizeof(g_conf.log_path), "%s", DEFAULT_LOG_PATH);
	}

	if (!ty_getconfstr(&sconf, "Log_name", g_conf.log_name))
	{
		ty_writelog(TY_LOG_WARNING, "<load_conf> can not load log_name");
		snprintf(g_conf.log_name, sizeof(g_conf.log_name), "%s", PROJECT_NAME);
	}

	if (!ty_getconfint(&sconf, "Log_event", &g_conf.log_event))
	{
		ty_writelog(TY_LOG_WARNING, "<load_conf> can not load log_event");
		g_conf.log_event = DEFAULT_LOG_EVENT;
	}

	if (!ty_getconfint(&sconf, "Log_other", &g_conf.log_other))
	{
		ty_writelog(TY_LOG_WARNING, "<load_conf> can not load log_other");
		g_conf.log_other = DEFAULT_LOG_OTHER;
	}

	if (!ty_getconfint(&sconf, "Log_size", &g_conf.log_size))
	{
		ty_writelog(TY_LOG_WARNING, "<load_conf> can not load log_size");
		g_conf.log_size = DEFAULT_LOG_SIZE;
	}

	return 0;
}


int init()
{
	// load_sysconf
	if (load_conf() < 0)
	{
		return -1;
	}
		
	signalsetup();

	// open logs
	Tylog_conf_t log_conf;
	log_conf.log_event = g_conf.log_event;
	log_conf.log_other = g_conf.log_other;
	log_conf.log_size = g_conf.log_size;

	ty_log_open(g_conf.log_path, g_conf.log_name, &log_conf);

	srand(time(NULL));

	g_pending_handle = ty_pending_creat();
	if (g_pending_handle == NULL)
	{
		ty_writelog(TY_LOG_FATAL, "<init> call ty_pending_creat failed!");
		return -1;
	}
	return 0;
}

void *cpool_thread(void *arg)
{
	int fdlis, maxfd, ret, sock_work;
	fd_set fs;
	bool bNewConn;
	int on = 1;
	
	ty_log_open_r("cpool_thread", NULL);

	if ((fdlis = ty_tcplisten(g_conf.listen_port, 256)) < 0) 
	{
		ty_writelog(TY_LOG_FATAL, "fail to listen on port[%d]!", g_conf.listen_port);
	    for(;;)
        	raise(SIGKILL);
       	 //exit(-1);
       	return NULL;
	}


	const timeval TV_TIMEOUT = {0, 10};
	timeval tv;
	
	while (1) 
	{
		FD_ZERO(&fs);
		FD_SET(fdlis, &fs);
		
		maxfd = ty_pending_mask(g_pending_handle, &fs);
		if (maxfd <= fdlis) 
		{
			maxfd = fdlis + 1;
		}

		tv = TV_TIMEOUT;
		ret = select(maxfd, &fs, NULL, NULL, &tv);
		if (ret <= 0)
			continue;

		if (FD_ISSET(fdlis, &fs))
			bNewConn = true;
		else
			bNewConn = false;
		
		if (bNewConn) 
		{
			sock_work = ty_accept(fdlis, NULL, NULL);
ty_writelog(TY_LOG_DEBUG, "<cpool_thread> accept sock[%d]", sock_work);
			if (sock_work > 0) 
			{
				setsockopt(sock_work, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
				if (ty_pending_insert_item(g_pending_handle, sock_work) == -1) 
				{
					close(sock_work);
					ty_writelog(TY_LOG_WARNING, "<cpool_thread> connection num overflow!");
				}
			} 
			else 
			{
				ty_writelog(TY_LOG_WARNING, "<cpool_thread> fail to accept connection!");
			}
		}

		ty_pending_check_item(g_pending_handle, &fs);
	}
	return NULL;
}


int main(int argc, char *argv[])
{
	main_parse_option(argc, argv);
	if (init() < 0)
	{
		ty_writelog(TY_LOG_FATAL, "fail to init!");
		return -1;
	}

	Thread_t *service_thrdlist = NULL;
	if ((service_thrdlist = (Thread_t *) calloc(g_conf.thread_count, sizeof(Thread_t))) == NULL) 
    {
          	ty_writelog(TY_LOG_FATAL, "Allocate memory for service threads error!");
        	return 0;
   	}

	pthread_t cpool_tid;
	pthread_create(&cpool_tid, NULL, cpool_thread, NULL);

	for (int i = 0; i < g_conf.thread_count; i++) 
	{
    	service_thrdlist[i].thrd_info.thrd_no = i;
        pthread_create(&(service_thrdlist[i].thrd), NULL,
        				service_thread, &(service_thrdlist[i].thrd_info));
		usleep(10);
    }
    pthread_join(cpool_tid, NULL);
 	for(int i = 0; i < g_conf.thread_count; i++)
		pthread_join(service_thrdlist[i].thrd, NULL);
	return 0;
}
