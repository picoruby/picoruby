all:
	cd src ; $(MAKE) all
	cd cli ; $(MAKE) all

clean:
	cd src ; $(MAKE) clean
	cd cli ; $(MAKE) clean

install:
	cp cli/mmrbc /usr/local/bin/mmrbc
