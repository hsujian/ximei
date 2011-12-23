CFLAGS= -g -Wall -D_REENTRANT -I. #-DNDEBUG
LIBFLAGS= -L$(LIB_PATH) -lpthread

OBJS=scgi_server.o net.o scgi.o

all	: scgi_server

scgi_server : $(OBJS)
	gcc -o $@ $^ $(CFLAGS) $(LIBFLAGS)

clean:
	rm -f *.o scgi_server *~

