CPPFLAGS= -Wl,--rpath -Wl,./ -O2 -Wall -DNDEBUG -D_REENTRANT #-g
LIBFLAGS= -lev

OBJS=pipeline_pool.o net.o

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

