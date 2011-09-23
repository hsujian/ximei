INC_PATH=$(HOME)/svn/search/trunk/all_search/as_lib
LIB_PATH=$(HOME)/svn/search/trunk/all_search/as_lib

CPPFLAGS= -O2 -Wall -D_REENTRANT -I$(INC_PATH) -I../ -pthread #-DNDEBUG
LIBFLAGS= -L../ -lgreeting_bonze -lrt -L$(LIB_PATH) -ltylib  -ltylog -lpthread

OBJS=main.o core.o

LIBO=test_serv

all	: $(LIBO)

$(LIBO) : $(OBJS)
	g++ -o $@ $^ $(CPPFLAGS) $(LIBFLAGS)

client : client.o
	g++ -o $@ $^ $(CPPFLAGS) $(LIBFLAGS)

clean:
	-rm -f *.o $(LIBO)
