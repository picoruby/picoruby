CC := gcc
AR := ar
GDB := gdb
CC_ARM := arm-linux-gnueabihf-gcc
AR_ARM := arm-linux-gnueabihf-ar
CC_PSOC := arm-none-eabi-gcc
AR_PSOC := arm-none-eabi-ar
CFLAGS += -Wall -Wpointer-arith -std=gnu99 -DHEAP_SIZE=1000000
LDFLAGS +=
LIB_DIR_PSOC5LP              := psoc5lp/lib
LIB_DIR_HOST_DEBUG_LIBC      := host-debug/alloc_libc
LIB_DIR_HOST_PRODUCTION_LIBC := host-production/alloc_libc
LIB_DIR_HOST_DEBUG_MRBC      := host-debug/alloc_mrbc
LIB_DIR_HOST_PRODUCTION_MRBC := host-production/alloc_mrbc
LIB_DIR_ARM_DEBUG_LIBC       := arm-debug/alloc_libc
LIB_DIR_ARM_PRODUCTION_LIBC  := arm-production/alloc_libc
LIB_DIR_ARM_DEBUG_MRBC       := arm-debug/alloc_mrbc
LIB_DIR_ARM_PRODUCTION_MRBC  := arm-production/alloc_mrbc
DEPS := \
  ../../src/common.h                        ../../src/common.c \
  ../../src/compiler.h                      ../../src/compiler.c \
  ../../src/generator.h                     ../../src/generator.c \
  ../../src/my_regex.h                      ../../src/my_regex.c \
  ../../src/node.h                          ../../src/node.c \
  ../../src/scope.h                         ../../src/scope.c \
  ../../src/stream.h                        ../../src/stream.c \
  ../../src/token.h                         ../../src/token.c \
  ../../src/dump.h                          ../../src/dump.c \
  ../../src/tokenizer.h                     ../../src/tokenizer.c \
  ../../src/regex_light/src/regex.h         ../../src/regex_light/src/regex.c \
  ../../src/ruby-lemon-parse/parse_header.h ../../src/ruby-lemon-parse/parse.y \
  ../../src/debug.h \
  ../../src/token_data.h \
  ../../src/version.h

host_all: host_debug host_production

host_debug: host_debug_libc host_debug_mrbc

host_production: host_production_libc host_production_mrbc

host_debug_libc:
	@mkdir -p $(LIB_DIR_HOST_DEBUG_LIBC)
	$(MAKE) libpicorbc LIB_DIR="$(LIB_DIR_HOST_DEBUG_LIBC)" \
	  CFLAGS="$(CFLAGS) -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

host_debug_mrbc:
	@mkdir -p $(LIB_DIR_HOST_DEBUG_MRBC)
	$(MAKE) libpicorbc LIB_DIR="$(LIB_DIR_HOST_DEBUG_MRBC)" \
	  CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

host_production_libc:
	@mkdir -p $(LIB_DIR_HOST_PRODUCTION_LIBC)
	$(MAKE) libpicorbc LIB_DIR="$(LIB_DIR_HOST_PRODUCTION_LIBC)" \
	  CFLAGS="$(CFLAGS) -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

host_production_mrbc:
	@mkdir -p $(LIB_DIR_HOST_PRODUCTION_MRBC)
	$(MAKE) libpicorbc LIB_DIR="$(LIB_DIR_HOST_PRODUCTION_MRBC)" \
	  CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

libpicorbc:
	@echo
	@echo "================================"
	@echo "building libpicorbc.a"
	@echo "  LIB_DIR=$(LIB_DIR)"
	cd ../../src ; \
	  $(MAKE) clean all CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
	  CC=$(CC) AR=$(AR)
	mv ../../src/*.o $(LIB_DIR)/
	mv ../../src/libpicorbc.a $(LIB_DIR)/libpicorbc.a

clean:
	rm -f $(LIB_DIR_PSOC5LP)/*.*
	rm -f $(LIB_DIR_HOST_DEBUG_LIBC)/*.*
	rm -f $(LIB_DIR_HOST_PRODUCTION_LIBC)/*.*
	rm -f $(LIB_DIR_HOST_DEBUG_MRBC)/*.*
	rm -f $(LIB_DIR_HOST_PRODUCTION_MRBC)/*.*
	rm -f $(LIB_DIR_ARM_DEBUG_LIBC)/*.*
	rm -f $(LIB_DIR_ARM_PRODUCTION_LIBC)/*.*
	rm -f $(LIB_DIR_ARM_DEBUG_MRBC)/*.*
	rm -f $(LIB_DIR_ARM_PRODUCTION_MRBC)/*.*
