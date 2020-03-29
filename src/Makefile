CFLAGS += -Wall -Wpointer-arith -std=gnu99
AR = ar
CC = gcc
TARGET := libmmrbc.a
SRCS := common.c token.c tokenizer.c generator.c my_regex.c scope.c node.c fmemstream.c
OBJS := $(SRCS:%.c=%.o)
DEPS := $(SRCS:%.c=%.d)

all: $(TARGET)

-include $(DEPS)

$(TARGET): ruby-lemon-parse/parse.o $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

ruby-lemon-parse/parse.o: ruby-lemon-parse/parse.y
	cd ruby-lemon-parse ; LEMON_MACRO=-DLEMON_MMRBC CFLAGS="$(CFLAGS)" $(MAKE) all

%.o: %.c
	$(CC) -c -MMD -MP $(CFLAGS) $<

clean:
	cd ruby-lemon-parse ; $(MAKE) clean
	rm -f $(TARGET) $(OBJS) $(DEPS)
