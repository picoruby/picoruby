CFLAGS += -Wall -Wpointer-arith -std=c99 -pedantic # -pedantic-errors
AR = ar
CC = gcc
TARGET = libmmrbc.a
SRCS = tokenizer.c generator.c
OBJS = $(SRCS:.c=.o)

all:
	$(MAKE) $(TARGET)

$(TARGET): ruby-lemon-parse/parse.o $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

ruby-lemon-parse/parse.c:
	cd ruby-lemon-parse ; $(MAKE) all

tokenizer.o: tokenizer.c

generator.o: generator.c

clean:
	cd ruby-lemon-parse ; $(MAKE) clean
	rm -f $(TARGET) $(OBJS)
