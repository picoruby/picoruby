CFLAGS += -Wall -Wpointer-arith -std=gnu99
CC = gcc
LDFLAGS +=
TARGETS = mmrbc mmruby
MRUBYCDIR = ../src/mrubyc/src
MMRBCDIR = ../src

all: $(TARGETS)

mmrbc: mmrbc.c heap.h ../src/libmmrbc.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

mmruby: mmruby.c heap.h ../src/libmmrbc.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

clean:
	@rm -f $(TARGETS)
