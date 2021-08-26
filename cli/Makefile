CC := gcc
AR := ar
CFLAGS += -Wall -Wpointer-arith -std=gnu99 -DHEAP_SIZE=1000000
LDFLAGS +=

all: picorbc picoruby picoirb

picorbc: picorbc.c heap.h
	@echo
	@echo "================================"
	@echo "building picorbc"
	@echo "  DIR=$(DIR)"
	mkdir -p ../build/bin/$(DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ../build/bin/$(DIR)/$@ $^ -L../build/lib/$(DIR) $(LIBS)

picoruby: picoruby.c heap.h
	@echo
	@echo "================================"
	@echo "building picoruby"
	@echo "  DIR=$(DIR)"
	mkdir -p ../build/bin/$(DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ../build/bin/$(DIR)/$@ $^ -L../build/lib/$(DIR) $(LIBS)

picoirb: picoirb.c heap.h sandbox.c
	$(MAKE) mrblib
	@echo
	@echo "================================"
	@echo "building picoirb"
	@echo "  DIR=$(DIR)"
	mkdir -p ../build/bin/$(DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o ../build/bin/$(DIR)/$@ $^ -L../build/lib/$(DIR) $(LIBS)

mrblib: ruby/irb.c ruby/sandbox.c ruby/buffer.c

ruby/irb.c: ruby/irb.rb
	$(MRBC) -B$(basename $(^F)) $<

ruby/sandbox.c: ruby/sandbox.rb
	$(MRBC) -B$(basename $(^F)) $<

ruby/buffer.c: ruby/buffer.rb
	$(MRBC) -B$(basename $(^F)) $<

picoshell: picoshell.c heap.h picoshell_lib/shell.c picoshell_lib/shell.rb
	$(CC) $(CFLAGS) $(LDFLAGS) \
	  -o $@ $(filter-out picoshell_lib/shell.c picoshell_lib/shell.rb, $^) \
	  -L$(LIB_DIR) -lmrubyc -lpicorbc

picoshell_lib/shell.c: picoshell_lib/shell.rb
	cd picoshell_lib ; make all PICORBC=$(PICORBC)

clean:
	cd picoshell_lib ; make clean
	rm ruby/*.c
