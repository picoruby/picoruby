CFLAGS += -Wall -Wpointer-arith -std=c99 -g -O0 -DDEBUG_BUILD # -Os
CC = gcc
LDFLAGS +=
TARGET = mmrbc
LIBMMRBC = ../src/libmmrbc.a

all: $(TARGET)

$(TARGET): main.c $(LIBMMRBC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(LIBMMRBC):
	cd ../src ; $(MAKE) all

clean:
	rm -f $(TARGET)
