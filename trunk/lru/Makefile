CC=gcc
CFLAGS= -O2 -W -Wall -D_REENTRANT #-DNDEBUG
LFLAGS=

OBJS=lru.o

LIBO=liblru.a

all	: $(LIBO)

$(LIBO) : $(OBJS)
	@-rm -f $@
	$(AR) rcv $@ $^
	@-rm -f $^
clean:
	@-rm -f *.o $(LIBO)
tags:
	ctags *.h *.c

