TARGETS = shell.c

all: $(TARGETS)

shell.c: shell.rb
	$(PICORBC) -B shell -o $@ shell.rb

clean:
	rm -f $(TARGETS)
