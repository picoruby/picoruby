CFLAGS += -Wall -Wpointer-arith -std=gnu99

debug:
	CFLAGS="-O0 -g3 $(CFLAGS)" $(MAKE) build_all

production:
	$(MAKE) clean
	CFLAGS="-Os -Wl,-s -DNDEBUG $(CFLAGS)" $(MAKE) all

build_all: mmruby mmirb

mmruby:
	cd src/mrubyc/src ; CFLAGS="$(CFLAGS)" $(MAKE) clean all
	cd src ;  CFLAGS="$(CFLAGS)" $(MAKE) all
	cd cli ; CFLAGS="$(CFLAGS)" $(MAKE) mmruby mmrbc

mmirb:
	cd src/mrubyc/src ; CFLAGS="-DMRBC_IRB $(CFLAGS)" $(MAKE) clean all
	cd src ; CFLAGS="$(CFLAGS)" $(MAKE) all
	cd cli ; CFLAGS="$(CFLAGS)" $(MAKE) mmirb

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
