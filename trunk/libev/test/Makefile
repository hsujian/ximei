INC_PATH=$(HOME)/svn/system_platform/search/trunk/all_search/as_lib
LIB_PATH=$(HOME)/svn/system_platform/search/trunk/all_search/as_lib

CPPFLAGS= -O2 -Wall -D_REENTRANT -I$(INC_PATH) -I../ #-DNDEBUG
LIBFLAGS= -lev -L../ -lpipeline_pool -L$(LIB_PATH) -ltylib  -ltylog -lpthread

OBJS=main.o core.o

LIBO=test_serv

all	: $(LIBO)

$(LIBO) : $(OBJS)
	g++ -o $@ $^ $(CPPFLAGS) $(LIBFLAGS)

clean:
	rm *.o
	rm $(LIBO)
