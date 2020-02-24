#CFLAGS += -Wall -Wpointer-arith -std=c99 -Os
CFLAGS += -Wall -Wpointer-arith -std=gnu99 -g -O0 -DDEBUG_BUILD
CC = gcc
LDFLAGS +=
TARGET = mmrbc
MRUBYCSRCDIR = ../src/mrubyc/src
PARSE = ../src/ruby-lemon-parse/parse

all: $(TARGET) $(PARSE).c

$(TARGET): main.c ../src/*.c $(PARSE).c $(MRUBYCSRCDIR)/*.c $(MRUBYCSRCDIR)/hal/*.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(PARSE).c: $(PARSE).y
	cd ../src/ruby-lemon-parse/ ; $(MAKE) all

clean:
	rm -f $(TARGET)
