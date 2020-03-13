CFLAGS += -Wall -Wpointer-arith -std=gnu99 -O0 -g3 -DDEBUG_BUILD #-Wl,-s

all:
	cd src/mrubyc/src ; CFLAGS="$(CFLAGS)" $(MAKE) all
	cd src ; CFLAGS="$(CFLAGS)" $(MAKE) all
	cd cli ; CFLAGS="$(CFLAGS)" $(MAKE) all

debug: all
	gdb --args ./cli/mmrbc test/fixtures/hello_world.rb

clean:
	cd src/mrubyc/src ; $(MAKE) clean
	cd src ; $(MAKE) clean
	cd cli ; $(MAKE) clean

install:
	cp cli/mmrbc /usr/local/bin/mmrbc
