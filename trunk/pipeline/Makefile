CPPFLAGS= -O2 -W -Wall -DNDEBUG -D_REENTRANT #-g
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

