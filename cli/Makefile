CFLAGS += -Wall -Wpointer-arith -g -std=c99 -pedantic # -pedantic-errors
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
