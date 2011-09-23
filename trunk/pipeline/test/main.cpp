#include "core.h"

Conf_t g_conf;
char g_sys_path[MAX_PATH_LEN];
greeting_bonze_t *g_pending_handle;


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

	snprintf(g_sys_path, sizeof(g_sys_path), "./");
	
	return 1;
}

int load_conf()
{
	g_conf.listen_port = 8080;
	g_conf.thread_count = DEFAULT_THREAD_NUM;
	snprintf(g_conf.log_path, sizeof(g_conf.log_path), "%s", DEFAULT_LOG_PATH);
	snprintf(g_conf.log_name, sizeof(g_conf.log_name), "%s", PROJECT_NAME);
	g_conf.log_event = DEFAULT_LOG_EVENT;
	g_conf.log_other = DEFAULT_LOG_OTHER;
	g_conf.log_size = DEFAULT_LOG_SIZE;

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

	g_pending_handle = greeting_bonze_new(2048, 128, MAX_RESPONSE_LEN);
	if (g_pending_handle == NULL)
	{
		ty_writelog(TY_LOG_FATAL, "<init> call pipeline_creat failed!");
		return -1;
	}
	greeting_bonze_set_guest_fn(g_pending_handle, service_once);
	return 0;
}

int main(int argc, char *argv[])
{
	main_parse_option(argc, argv);
	if (init() < 0)
	{
		ty_writelog(TY_LOG_FATAL, "fail to init!");
		return -1;
	}

	greeting_bonze_listen_port(g_pending_handle, g_conf.listen_port);
	greeting_bonze_do(g_pending_handle);
	return 0;
	Thread_t *service_thrdlist = NULL;
	if ((service_thrdlist = (Thread_t *) calloc(g_conf.thread_count, sizeof(Thread_t))) == NULL) 
    {
          	ty_writelog(TY_LOG_FATAL, "Allocate memory for service threads error!");
        	return 0;
   	}

	for (int i = 0; i < g_conf.thread_count; i++) 
	{
    	service_thrdlist[i].thrd_info.thrd_no = i;
        pthread_create(&(service_thrdlist[i].thrd), NULL,
        				service_thread, &(service_thrdlist[i].thrd_info));
    }
	greeting_bonze_do(g_pending_handle);
 	for(int i = 0; i < g_conf.thread_count; i++)
		pthread_join(service_thrdlist[i].thrd, NULL);
	return 0;
}
