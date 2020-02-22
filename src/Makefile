#CFLAGS += -Wall -Wpointer-arith -std=c99 -Os
CFLAGS += -Wall -Wpointer-arith -std=c99 -g -O0 -DDEBUG_BUILD
AR = ar
CC = gcc
TARGET = libmmrbc.a
SRCS = common.c token.c tokenizer.c generator.c my_regex.c
OBJS = $(SRCS:.c=.o)

all:
	$(MAKE) $(TARGET)

$(TARGET): ruby-lemon-parse/parse.o $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

ruby-lemon-parse/parse.c:
	cd ruby-lemon-parse ; $(MAKE) all

token.o: common.h debug.h mmrbc.h token.h token.c

tokenizer.o: token_data.h common.h debug.h mmrbc.h token.h token.c tokenizer.h tokenizer.c

generator.o: common.h debug.h mmrbc.h generator.h generator.c

my_regex.o: common.h debug.h mmrbc.h my_regex.h my_regex.c

common.o: common.h debug.h common.c

clean:
	cd ruby-lemon-parse ; $(MAKE) clean
	rm -f $(TARGET) $(OBJS)
