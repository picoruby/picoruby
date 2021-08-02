CC := gcc
AR := ar
GDB := gdb
CC_ARM := arm-linux-gnueabihf-gcc
AR_ARM := arm-linux-gnueabihf-ar
CC_PSOC := arm-none-eabi-gcc
AR_PSOC := arm-none-eabi-ar
CFLAGS += -Wall -Wpointer-arith -std=gnu99 -DHEAP_SIZE=1000000
LDFLAGS +=

all: picorbc picoruby picoirb

picorbc: picorbc.c heap.h
	mkdir -p ../build/bin/$(DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ../build/bin/$(DIR)/$@ $^ -L../build/lib/$(DIR) -lpicorbc

picoruby: picoruby.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN_DIR)/$@ $^ -L$(LIB_DIR) -lmrubyc -lpicorbc

picoirb: picoirb.c heap.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN_DIR)/$@ $^ -L$(LIB_DIR) -lmrubyc -lpicorbc

picoshell: picoshell.c heap.h picoshell_lib/shell.c picoshell_lib/shell.rb
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(filter-out picoshell_lib/shell.c picoshell_lib/shell.rb, $^) -L../$(LIB_DIR) -lmrubyc -lpicorbc

picoshell_lib/shell.c: picoshell_lib/shell.rb
	cd picoshell_lib ; make all

clean:
	cd picoshell_lib ; make clean
	rm -f $(TARGETS)
