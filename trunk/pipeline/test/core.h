#ifndef __SERVER_CORE_H_
#define __SERVER_CORE_H_

#include <signal.h>


#include "define.h"
#include "interface.h"

typedef struct tag_Conf_t 
{
	int thread_count;              //模块线程数

	// socket	
	int listen_port;                 //侦听端口
	int read_tmout;
	int write_tmout;

	// log file
	char log_path[MAX_PATH_LEN];
	char log_name[WORD_SIZE];
	int log_event;
	int log_other;
	int log_size;

	// other

}Conf_t;


typedef struct tag_Log_info_t
{
	int status;
	u_int ip;
}Log_info_t;


typedef struct tag_Thread_info_t
{
	int thrd_no;
}Thread_info_t;

typedef struct tag_Tread_t
{
	pthread_t thrd;
	Thread_info_t thrd_info;
}Thread_t;

#define OK  			200
#define UI_GET_ERROR  	301
#define UI_PUT_ERROR   	302


void signalsetup();

void *service_thread(void *pti);


#endif
