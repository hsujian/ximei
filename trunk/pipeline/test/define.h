#ifndef __SERVER_DEFINE_H_
#define __SERVER_DEFINE_H_

#include <sys/time.h>

#include "ty_log.h"
#include "ty_conf.h"
#include "ty_net.h"
#include "ty_dict.h"
#include "ty_pending_pool.h"

#define PROJECT_NAME 	"server"
#define SERVER_VERSION  "1.0"

#define DEFAULT_THREAD_NUM 40
#define DEFAULT_IP_TYPE 1

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 256
#endif

#define DEFAULT_LOG_PATH "../log"
#define DEFAULT_LOG_EVENT 0xff
#define DEFAULT_LOG_OTHER 0xff
#define DEFAULT_LOG_SIZE 64000000

#define GetTimeCurrent(tv) gettimeofday(&tv, NULL)
#define SetTimeUsed(tused, tv1, tv2) \
{ \ 
	    tused  = (tv2.tv_sec-tv1.tv_sec) * 1000; \
        tused += (tv2.tv_usec-tv1.tv_usec) / 1000 + 1; \
}


#endif
