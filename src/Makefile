CFLAGS += -Wall -Wpointer-arith -std=gnu99
AR := ar
CC := gcc
TARGET := libmmrbc.a
SRCS := compiler.c common.c token.c tokenizer.c generator.c my_regex.c scope.c node.c stream.c
OBJS := $(SRCS:%.c=%.o)
DEPS := $(SRCS:%.c=%.d)

all: $(TARGET)

-include $(DEPS)

$(TARGET): ruby-lemon-parse/parse.o $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

ruby-lemon-parse/parse.o: ruby-lemon-parse/parse.y
	cd ruby-lemon-parse ; LEMON_MACRO=-DLEMON_MMRBC $(MAKE) all CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

%.o: %.c
	$(CC) -c -MMD -MP $(CFLAGS) $(LDFLAGS) $<

clean:
	cd ruby-lemon-parse ; $(MAKE) clean
	cd mrubyc/src ; $(MAKE) clean
	rm -f $(TARGET) $(OBJS) $(DEPS)
