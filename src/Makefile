CFLAGS += -Wall -Wpointer-arith -std=gnu99
AR := ar
CC := gcc
TARGET := libmmrbc.a
SRCS := compiler.c common.c token.c tokenizer.c generator.c my_regex.c regex_light.c scope.c node.c stream.c
OBJS := $(SRCS:%.c=%.o)
DEPS := $(SRCS:%.c=%.d)

all: tokenizer_helper.h ruby-lemon-parse/parse.c $(TARGET)

-include $(DEPS)

tokenizer_helper.h: token.h tokenizer.h
	./helper_gen.rb tokenizer

ruby-lemon-parse/parse.c:
	cd ruby-lemon-parse ; \
	  LEMON_MACRO=-DLEMON_MMRBC $(MAKE) parse.c atom_helper.h token_helper.h keyword_helper.h

$(TARGET): $(OBJS)
ifeq ($(findstring MMRUBY_REGEX_LIBC, $(CFLAGS)), MMRUBY_REGEX_LIBC)
	$(AR) $(ARFLAGS) $@ $(filter-out regex_light.o, $^)
else
	$(AR) $(ARFLAGS) $@ $^
endif

%.o: %.c
	$(CC) -c -MMD -MP $(CFLAGS) $(LDFLAGS) $<

clean:
	cd ruby-lemon-parse ; $(MAKE) clean
	cd mrubyc/src ; $(MAKE) clean
	rm -f $(TARGET) $(OBJS) $(DEPS) tokenizer_helper.h
