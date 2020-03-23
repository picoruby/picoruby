CFLAGS += -Wall -Wpointer-arith -std=gnu99

debug:
	CFLAGS="-O0 -g3 -DDEBUG_BUILD" $(MAKE) all

production:
	$(MAKE) clean
	CFLAGS="-Os -Wl,-s" $(MAKE) all

all:
	cd src/mrubyc/src ; CFLAGS="$(CFLAGS)" $(MAKE) all
	cd src ; CFLAGS="$(CFLAGS)" $(MAKE) all
	cd cli ; CFLAGS="$(CFLAGS)" $(MAKE) all


gdb: debug
	gdb --args ./cli/mmrbc test/fixtures/hello_world.rb

clean:
	cd src/mrubyc/src ; $(MAKE) clean
	cd src ; $(MAKE) clean
	cd cli ; $(MAKE) clean

install:
	cp cli/mmrbc /usr/local/bin/mmrbc
