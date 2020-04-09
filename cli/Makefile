#CFLAGS += -Wall -Wpointer-arith -std=gnu99
CC = gcc
LDFLAGS +=
TARGETS = mmrbc mmruby mmirb_lib/shell.c mmirb
MRUBYCDIR = ../src/mrubyc/src
MMRBCDIR = ../src

all:
	@echo "Target `all` doesn't work"

mmrbc: mmrbc.c heap.h ../src/libmmrbc.a ../src/mrubyc/src/libmrubyc.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

mmruby: mmruby.c heap.h ../src/libmmrbc.a ../src/mrubyc/src/libmrubyc.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

mmirb: mmirb.c heap.h ../src/libmmrbc.a ../src/mrubyc/src/libmrubyc.a mmirb_lib/shell.c mmirb_lib/shell.rb
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(filter-out mmirb_lib/shell.c mmirb_lib/shell.rb, $^) -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

mmirb_lib/shell.c: mmirb_lib/shell.rb
	cd mmirb_lib ; make all

clean:
	rm -f $(TARGETS)
	cd mmirb_lib ; make clean
