CFLAGS += -Wall -Wpointer-arith -std=gnu99
CC := gcc
LDFLAGS +=
TARGETS = mmrbc mmruby mmirb_lib/shell.c mmirb
MRUBYCDIR = ../src/mrubyc/src
MMRBCDIR = ../src

all: mmrbc mmruby mmirb

mmrbc: mmrbc.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L../$(LIB_DIR) -lmrubyc -lmmrbc

mmruby: mmruby.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L../$(LIB_DIR) -lmrubyc -lmmrbc

mmirb: mmirb.c heap.h mmirb_lib/shell.c mmirb_lib/shell.rb
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(filter-out mmirb_lib/shell.c mmirb_lib/shell.rb, $^) -L../$(LIB_DIR) -lmrubyc -lmmrbc

mmirb_lib/shell.c: mmirb_lib/shell.rb
	cd mmirb_lib ; make all

clean:
	cd mmirb_lib ; make clean
	rm -f $(TARGETS)
