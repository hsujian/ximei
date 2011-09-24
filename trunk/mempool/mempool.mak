CC=gcc

CFLAGS= -O2 -g -Wall -D_REENTRANT -DLINUX -DNDEBUG

INCLUDE=-I./
LIBPATH=-L./
LIBLINK=-lpthread -lmempool 

TARGET=libmempool.a
OBJS=mempool.o

all: $(TARGET)

.c.o:
	$(CC) $(CFLAGS) -c $< $(INCLUDE)
.cpp.o:
	$(CC) $(CFLAGS) -c $< $(INCLUDE) 

$(TARGET): $(OBJS)
	ar -rcs $@ $^

test: test.o
	$(CC) -o $@ $^ $(LIBPATH) $(LIBLINK)
single: single.o
	$(CC) -o $@ $^ $(LIBPATH) $(LIBLINK)
multiple: multiple.o
	$(CC) -o $@ $^ $(LIBPATH) $(LIBLINK)
.PHONY: clean cleanall install
clean:
	rm -f *.o
cleanall:
	rm -f *.o *~ $(TARGET) single multiple
install:
	cp mempool.h $(HOME)/include
	cp libmempool.a $(HOME)/lib

