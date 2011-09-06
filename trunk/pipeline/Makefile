CC=g++
CPPFLAGS= -O2 -W -Wall -D_REENTRANT -I$(HOME)/svn/system_platform/search/trunk/all_search/as_lib #-DNDEBUG
LIBFLAGS= -lev

OBJS=net.o pipeline_pool.o

LIBO=libpipeline_pool.a

all	: $(LIBO)

$(LIBO) : $(OBJS)
	@-rm -f $@
	$(AR) rcv $@ $^
	@-rm -f $^
clean:
	@-rm -f *.o $(LIBO)
tags:
	ctags *.h *.c

