CFLAGS += -Wall -Wpointer-arith -std=c99 -g -O0 -DDEBUG_BUILD # -Os
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

token.o: debug.h mmrbc.h token.h token.c

tokenizer.o: debug.h mmrbc.h token.h token.c tokenizer.h tokenizer.c

generator.o: debug.h mmrbc.h generator.h generator.c

my_regex.o: debug.h mmrbc.h my_regex.h my_regex.c

clean:
	cd ruby-lemon-parse ; $(MAKE) clean
	rm -f $(TARGET) $(OBJS)
