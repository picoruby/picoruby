TARGETS = shell.c

all: $(TARGETS)

shell.c: shell.rb
	mrbc -E -Bshell -o $@ shell.rb
	touch shell.h

clean:
	rm -f $(TARGETS) shell.h
