CFLAGS += -Wall -Wpointer-arith -std=gnu99
CC = gcc
LDFLAGS +=
TARGET = mmrbc
MRUBYCDIR = ../src/mrubyc/src
MMRBCDIR = ../src
PARSE = ../src/ruby-lemon-parse/parse

all: $(TARGET)

$(TARGET): main.c $(PARSE).h $(MMRBCDIR)/libmmrbc.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

$(PARSE).h:
	cd $(MMRBCDIR) ; make all

$(MMRBCDIR)/libmmrbc.a:
	cd $(MMRBCDIR) ; make all

clean:
	rm -f $(TARGET)
