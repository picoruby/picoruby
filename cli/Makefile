CFLAGS += -Wall -Wpointer-arith -std=gnu99
CC = gcc
LDFLAGS +=
TARGETS = mmrbc mrubyc
MRUBYCDIR = ../src/mrubyc/src
MMRBCDIR = ../src

all: $(TARGETS)

mmrbc: mmrbc.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

mrubyc: mrubyc.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

clean:
	@rm -f $(TARGETS)
