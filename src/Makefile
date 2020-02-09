CFLAGS += -Wall -Wpointer-arith -std=c99 # -Os # -pedantic # -pedantic-errors
AR = ar
CC = gcc
TARGET = libmmrbc.a
SRCS = token.c tokenizer.c generator.c my_regex.c
OBJS = $(SRCS:.c=.o)

all:
	$(MAKE) $(TARGET)

$(TARGET): ruby-lemon-parse/parse.o $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

ruby-lemon-parse/parse.c:
	cd ruby-lemon-parse ; $(MAKE) all

token.o: token.h token.c

tokenizer.o: token.h token.c tokenizer.h tokenizer.c

generator.o: generator.h generator.c

my_regex.o: my_regex.h my_regex.c

clean:
	cd ruby-lemon-parse ; $(MAKE) clean
	rm -f $(TARGET) $(OBJS)
