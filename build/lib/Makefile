CC := gcc
AR := ar
CC_ARM := arm-linux-gnueabihf-gcc
AR_ARM := arm-linux-gnueabihf-ar
CC_PSOC := arm-none-eabi-gcc
AR_PSOC := arm-none-eabi-ar
CFLAGS += -Wall -Wpointer-arith -std=gnu99 -DHEAP_SIZE=1000000
LDFLAGS +=
LIB_DIR_HOST_DEBUG_LIBC      := host-debug/alloc_libc
LIB_DIR_HOST_PRODUCTION_LIBC := host-production/alloc_libc
LIB_DIR_HOST_DEBUG_MRBC      := host-debug/alloc_mrbc
LIB_DIR_HOST_PRODUCTION_MRBC := host-production/alloc_mrbc
LIB_DIR_ARM_DEBUG_LIBC       := arm-debug/alloc_libc
LIB_DIR_ARM_PRODUCTION_LIBC  := arm-production/alloc_libc
LIB_DIR_ARM_DEBUG_MRBC       := arm-debug/alloc_mrbc
LIB_DIR_ARM_PRODUCTION_MRBC  := arm-production/alloc_mrbc
LIB_DIR_PSOC5LP_MRBC         := psoc5lp/alloc_mrbc

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

MRUBYC_DEPS := \
  ../../src/mrubyc/src/alloc.c \
  ../../src/mrubyc/src/c_array.c \
  ../../src/mrubyc/src/c_hash.c \
  ../../src/mrubyc/src/c_math.c \
  ../../src/mrubyc/src/c_numeric.c \
  ../../src/mrubyc/src/c_object.c \
  ../../src/mrubyc/src/c_range.c \
  ../../src/mrubyc/src/c_string.c \
  ../../src/mrubyc/src/class.c \
  ../../src/mrubyc/src/console.c \
  ../../src/mrubyc/src/error.c \
  ../../src/mrubyc/src/global.c \
  ../../src/mrubyc/src/keyvalue.c \
  ../../src/mrubyc/src/load.c \
  ../../src/mrubyc/src/mrblib.c \
  ../../src/mrubyc/src/rrt0.c \
  ../../src/mrubyc/src/symbol.c \
  ../../src/mrubyc/src/value.c \
  ../../src/mrubyc/src/vm.c

host_all: host_debug host_production

host_debug:      host_debug_libc      host_debug_mrbc
host_production: host_production_libc host_production_mrbc

host_debug_libc:      $(LIB_DIR_HOST_DEBUG_LIBC)_libs
host_debug_mrbc:      $(LIB_DIR_HOST_DEBUG_MRBC)_libs
host_production_libc: $(LIB_DIR_HOST_PRODUCTION_LIBC)_libs
host_production_mrbc: $(LIB_DIR_HOST_PRODUCTION_MRBC)_libs
arm_debug_libc:       $(LIB_DIR_ARM_DEBUG_LIBC)_libs
arm_debug_mrbc:       $(LIB_DIR_ARM_DEBUG_MRBC)_libs
arm_production_libc:  $(LIB_DIR_ARM_PRODUCTION_LIBC)_libs
arm_production_mrbc:  $(LIB_DIR_ARM_PRODUCTION_MRBC)_libs
psoc5lp_mrbc:         $(LIB_DIR_PSOC5LP_MRBC)_libs

$(LIB_DIR_HOST_DEBUG_LIBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_HOST_DEBUG_LIBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_HOST_DEBUG_LIBC)" \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_ALLOC_LIBC -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

$(LIB_DIR_HOST_DEBUG_MRBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_HOST_DEBUG_MRBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_HOST_DEBUG_MRBC)" \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

$(LIB_DIR_HOST_PRODUCTION_LIBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_HOST_PRODUCTION_LIBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_HOST_PRODUCTION_LIBC)" \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_ALLOC_LIBC -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

$(LIB_DIR_HOST_PRODUCTION_MRBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_HOST_PRODUCTION_MRBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_HOST_PRODUCTION_MRBC)" \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

$(LIB_DIR_ARM_DEBUG_LIBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_ARM_DEBUG_LIBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_ARM_DEBUG_LIBC)" \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_ALLOC_LIBC -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC_ARM) AR=$(AR_ARM)

$(LIB_DIR_ARM_DEBUG_MRBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_ARM_DEBUG_MRBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_ARM_DEBUG_MRBC)" \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC_ARM) AR=$(AR_ARM)

$(LIB_DIR_ARM_PRODUCTION_LIBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_ARM_PRODUCTION_LIBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_ARM_PRODUCTION_LIBC)" \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_ALLOC_LIBC -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC_ARM) AR=$(AR_ARM)

$(LIB_DIR_ARM_PRODUCTION_MRBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_ARM_PRODUCTION_MRBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_ARM_PRODUCTION_MRBC)" \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC_ARM) AR=$(AR_ARM)

$(LIB_DIR_PSOC5LP_MRBC)_libs: $(DEPS)
	@mkdir -p $(LIB_DIR_PSOC5LP_MRBC)
	$(MAKE) $(LIB) LIB_DIR="$(LIB_DIR_PSOC5LP_MRBC)" \
	  HAL_DIR=hal_psoc5lp \
	  COMMON_SRCS="alloc.c class.c console.c error.c global.c keyvalue.c load.c rrt0.c static.c symbol.c value.c vm.c" \
	  CFLAGS="$(CFLAGS) -I../../../include/psoc5lp -mcpu=cortex-m3 -mthumb -g -ffunction-sections -ffat-lto-objects -O0 -DNDEBUG -DMRBC_USE_HAL_PSOC5LP -DPTR_SIZE=4" \
	  LDFLAGS="$(LDFLAGS)" CC=$(CC_PSOC) AR=$(AR_PSOC)

libpicorbc: $(DEPS)
	@echo
	@echo "================================"
	@echo "building libpicorbc.a"
	@echo "  LIB_DIR=$(LIB_DIR)"
	cd ../../src ; \
	  $(MAKE) clean all CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
	  CC=$(CC) AR=$(AR)
	mv ../../src/*.o $(LIB_DIR)/
	mv ../../src/libpicorbc.a $(LIB_DIR)/libpicorbc.a
	@echo "\e[32;1mYey! $@\e[m\n"

libmrubyc: $(MRUBYC_DEPS)
	@echo
	@echo "================================"
	@echo "building libmrubyc.a"
	@echo "  LIB_DIR=$(LIB_DIR)"
	cd ../../src/mrubyc/src ; \
	  $(MAKE) all CFLAGS="$(CFLAGS) -DMRBC_CONVERT_CRLF" \
	  LDFLAGS="$(LDFLAGS)" HAL_DIR="$(HAL_DIR)" \
	  CC=$(CC) AR=$(AR)
	mv ../../src/mrubyc/src/*.o $(LIB_DIR)/
	mv ../../src/mrubyc/src/libmrubyc.a $(LIB_DIR)/libmrubyc.a
	@echo "\e[32;1mYey! $@\e[m\n"


../../src/mrubyc/src/hal_user_reerved/hal.c:
	cd ../../src/mrubyc/src/hal_user_reserved/ ;\
	  if [ ! -f ./hal.c ]; then ln -s ../../../../cli/picoshell_lib/hal_posix/hal.c ./hal.c; fi; \
	  if [ ! -f ./hal.h ]; then ln -s ../../../../cli/picoshell_lib/hal_posix/hal.h ./hal.h; fi

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
	rm -f $(LIB_DIR_PSOC5LP_MRBC)/*.*
