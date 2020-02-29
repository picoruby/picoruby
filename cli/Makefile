CFLAGS += -Wall -Wpointer-arith -std=gnu99
CC = gcc
LDFLAGS +=
TARGET = mmrbc
MRUBYCDIR = ../src/mrubyc/src
MMRBCDIR = ../src

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) $(LDFLAGS)-o $@ $^ -L$(MRUBYCDIR) -L$(MMRBCDIR) -lmrubyc -lmmrbc

clean:
	rm -f $(TARGET)
