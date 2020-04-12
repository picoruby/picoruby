CC := gcc
CC_ARM := arm-linux-gnueabihf-gcc
CFLAGS += -Wall -Wpointer-arith -std=gnu99
LDFLAGS += -static

debug:
	$(MAKE) build_all CC=$(CC) CFLAGS="-O0 -g3 $(CFLAGS)" LDFLAGS="$(LDFLAGS)"

production: clean
	$(MAKE) build_all CC=$(CC) CFLAGS="-Os -DNDEBUG $(CFLAGS)" LDFLAGS="-Wl,-s $(LDFLAGS)"

arm_debug: clean
	$(MAKE) debug CC=$(CC_ARM) CFLAGS="-O0 -g3 $(CFLAGS)" LDFLAGS="$(LDFLAGS)"

arm_production:
	$(MAKE) production CC=$(CC_ARM) CFLAGS="-Os -DNDEBUG $(CFLAGS)" LDFLAGS="-Wl,-s $(LDFLAGS)"

build_all: mmruby mmirb

mmruby:
	cd src/mrubyc/src ; $(MAKE) clean all CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd src ; $(MAKE) all CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) mmruby mmrbc CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

mmirb:
	cd src/mrubyc/src ; $(MAKE) clean all CFLAGS="-DMRBC_IRB $(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd src ; $(MAKE) all CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) mmirb CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

gdb: debug
	gdb --args ./cli/mmrbc test/fixtures/hello_world.rb

irb:
	@cd bin ; bundle exec ruby mmirb.rb

kill-irb:
	bin/kill.sh

clean:
	cd src/mrubyc/src ; $(MAKE) clean
	cd src ; $(MAKE) clean
	cd cli ; $(MAKE) clean

install:
	cp cli/mmrbc /usr/local/bin/mmrbc
	cp cli/mmruby /usr/local/bin/mmruby
