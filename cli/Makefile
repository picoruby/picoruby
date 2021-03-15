CFLAGS += -Wall -Wpointer-arith -std=gnu99
CC := gcc
LDFLAGS +=
TARGETS = picorbc picoruby picoirb_lib/shell.c picoirb
MRUBYCDIR = ../src/mrubyc/src
PICORBCDIR = ../src

all: picorbc picoruby picoirb

picorbc: picorbc.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L../$(LIB_DIR) -lmrubyc -lpicorbc

picoruby: picoruby.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L../$(LIB_DIR) -lmrubyc -lpicorbc

picoirb: picoirb.c heap.h picoirb_lib/shell.c picoirb_lib/shell.rb
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(filter-out picoirb_lib/shell.c picoirb_lib/shell.rb, $^) -L../$(LIB_DIR) -lmrubyc -lpicorbc

picoirb_lib/shell.c: picoirb_lib/shell.rb
	cd picoirb_lib ; make all

clean:
	cd picoirb_lib ; make clean
	rm -f $(TARGETS)
