all:
	cd src ; $(MAKE) all
	cd cli ; $(MAKE) all

debug: all
	gdb --args ./cli/mmrbc test/fixtures/hello.rb


clean:
	cd src ; $(MAKE) clean
	cd cli ; $(MAKE) clean

install:
	cp cli/mmrbc /usr/local/bin/mmrbc
