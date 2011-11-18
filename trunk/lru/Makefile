CC=gcc
CFLAGS= -O2 -Wall -D_REENTRANT -DNDEBUG
LFLAGS=

OBJS=lru.o

LIBO=liblru.a

all	: $(LIBO)

$(LIBO) : $(OBJS)
	@-rm -f $@
	$(AR) rcv $@ $^
clean:
	@-rm -f *.o $(LIBO)
tags:
	ctags *.h *.c

