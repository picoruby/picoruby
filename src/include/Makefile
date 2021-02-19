all:
	$(MAKE) clean
	$(CC) $(CFLAGS) -static -o ptr_size_generator ptr_size_generator.c && \
	  ./ptr_size_generator

clean:
	rm -f ptr_size_generator ptr_size.h
