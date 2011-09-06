#ifndef __UI_INTERFACE_H_
#define __UI_INTERFACE_H_

#include "ty_log.h"

typedef struct tag_TySock_head_t
{
	int status;
	short trace[32];
	u_int length;
	u_int ip;
	u_int need_pb 		:1;
	u_int need_merge 	:1;
	u_int other			:30;
}TySock_head_t;



#endif
