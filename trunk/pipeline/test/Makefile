INC_PATH=$(HOME)/tylib/include
LIB_PATH=$(HOME)/tylib/lib

CPPFLAGS= -O2 -Wall -DNDEBUG -D_REENTRANT -I$(INC_PATH)
LIBFLAGS= -L$(LIB_PATH) -ltylib  -ltylog -lpthread

OBJS=main.o core.o

LIBO=test_serv

all	: $(LIBO)

$(LIBO) : $(OBJS)
	g++ -o $@ $^ $(CPPFLAGS) $(LIBFLAGS)

clean:
	rm *.o
	rm $(LIBO)
