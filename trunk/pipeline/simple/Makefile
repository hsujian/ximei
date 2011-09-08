INC_PATH=$(HOME)/svn/system_platform/search/trunk/all_search/as_lib
LIB_PATH=$(HOME)/svn/system_platform/search/trunk/all_search/as_lib

CPPFLAGS= -O2 -g -Wall -D_REENTRANT -I$(INC_PATH) -I../ #-DNDEBUG
LIBFLAGS= -L$(LIB_PATH) -ltylib  -ltylog -lpthread

OBJS=server.o ../net.o

all	: serv cli

serv : server.o ../net.o
	g++ -o $@ $^ $(CPPFLAGS) $(LIBFLAGS)

cli : client.o ../net.o
	g++ -o $@ $^ $(CPPFLAGS) $(LIBFLAGS)

clean:
	rm -f *.o ../net.o serv cli

