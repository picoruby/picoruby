CC := gcc
AR := ar
GDB := gdb
CC_ARM := arm-linux-gnueabihf-gcc
AR_ARM := arm-linux-gnueabihf-ar
CC_PSOC := arm-none-eabi-gcc
AR_PSOC := arm-none-eabi-ar
CFLAGS += -Wall -Wpointer-arith -std=gnu99
LDFLAGS +=
LIB_DIR_PSOC5LP         := build/psoc5lp/lib
LIB_DIR_HOST_DEBUG      := build/host-debug/lib
LIB_DIR_HOST_PRODUCTION := build/host-production/lib
BIN_DIR_HOST_DEBUG      := build/host-debug/bin
BIN_DIR_HOST_PRODUCTION := build/host-production/bin
LIB_DIR_ARM_DEBUG       := build/arm-debug/lib
LIB_DIR_ARM_PRODUCTION  := build/arm-production/lib
BIN_DIR_ARM_DEBUG       := build/arm-debug/bin
BIN_DIR_ARM_PRODUCTION  := build/arm-production/bin
TEST_FILE := test/fixtures/hello_world.rb
DEPS := cli/heap.h cli/mmirb.c cli/mmruby.c cli/mmrbc.c \
        cli/mmirb_lib/shell.rb \
        src/common.h    src/common.c \
        src/compiler.h  src/compiler.c \
        src/debug.h \
        src/generator.h src/generator.c \
        src/my_regex.h  src/my_regex.c \
        src/regex_light.h  src/regex_light.c \
        src/node.h      src/node.c \
        src/scope.h     src/scope.c \
        src/stream.h    src/stream.c \
        src/token_data.h \
        src/token.h     src/token.c \
        src/tokenizer.h src/tokenizer.c \
        src/version.h \
        src/ruby-lemon-parse/parse_header.h src/ruby-lemon-parse/parse.y \
        src/ruby-lemon-parse/crc.c
TARGETS = $(BIN_DIR)/mmrbc $(BIN_DIR)/mmruby $(BIN_DIR)/mmirb

default: host_debug

all: host_all arm_all

host_all:
	$(MAKE) host_debug host_production \
	  CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
	  CC=$(CC) AR=$(AR)

host_debug:
	@mkdir -p $(LIB_DIR_HOST_DEBUG)
	@mkdir -p $(BIN_DIR_HOST_DEBUG)
	$(MAKE) $(BIN_DIR_HOST_DEBUG)/mmirb \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" LDFLAGS="$(LDFLAGS)" \
	  LIB_DIR=$(LIB_DIR_HOST_DEBUG) \
	  BIN_DIR=$(BIN_DIR_HOST_DEBUG) \
	  CC=$(CC) AR=$(AR)

host_production:
	@mkdir -p $(LIB_DIR_HOST_PRODUCTION)
	@mkdir -p $(BIN_DIR_HOST_PRODUCTION)
	$(MAKE) $(BIN_DIR_HOST_PRODUCTION)/mmirb \
	  CFLAGS="-Os -DNDEBUG -Wl,-s $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" LDFLAGS="$(LDFLAGS)" \
	  LIB_DIR=$(LIB_DIR_HOST_PRODUCTION) \
	  BIN_DIR=$(BIN_DIR_HOST_PRODUCTION) \
	  CC=$(CC) AR=$(AR)

arm_all:
	$(MAKE) arm_debug arm_production \
	  CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
	  CC=$(CC_ARM) AR=$(AR_ARM)

arm_debug:
	mkdir -p $(LIB_DIR_ARM_DEBUG)
	mkdir -p $(BIN_DIR_ARM_DEBUG)
	$(MAKE) $(BIN_DIR_ARM_DEBUG)/mmirb \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" LDFLAGS="$(LDFLAGS)" \
	  LIB_DIR=$(LIB_DIR_ARM_DEBUG) \
	  BIN_DIR=$(BIN_DIR_ARM_DEBUG) \
	  CC=$(CC_ARM) AR=$(AR_ARM)

arm_production:
	mkdir -p $(LIB_DIR_ARM_PRODUCTION)
	mkdir -p $(BIN_DIR_ARM_PRODUCTION)
	$(MAKE) $(BIN_DIR_ARM_PRODUCTION)/mmirb \
	  CFLAGS="-static -Os -DNDEBUG -Wl,-s $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" LDFLAGS="$(LDFLAGS)" \
	  LIB_DIR=$(LIB_DIR_ARM_PRODUCTION) \
	  BIN_DIR=$(BIN_DIR_ARM_PRODUCTION) \
	  CC=$(CC_ARM) AR=$(AR_ARM)

$(TARGETS): $(DEPS)
	$(MAKE) build_lib \
	  HAL_DIR=hal_user_reserved \
	  CFLAGS="$(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" LIB_DIR=$(LIB_DIR) \
	  CC=$(CC) AR=$(AR)
	$(MAKE) build_bin CFLAGS="$(CFLAGS)" \
	  LDFLAGS="$(LDFLAGS)" BIN_DIR=$(BIN_DIR) \
	  CC=$(CC) AR=$(AR)

psoc5lp_lib: $(DEPS)
	mkdir -p $(LIB_DIR_PSOC5LP)
	$(MAKE) build_lib \
	  HAL_DIR=hal_psoc5lp \
	  CFLAGS="$(CFLAGS) -I../../../include/psoc5lp -mcpu=cortex-m3 -mthumb -g -ffunction-sections -ffat-lto-objects -O0 -DNDEBUG" \
	  LDFLAGS=$(LDFLAGS) \
	  LIB_DIR=$(LIB_DIR_PSOC5LP) \
	  COMMON_SRCS="alloc.c class.c console.c error.c global.c keyvalue.c load.c rrt0.c static.c symbol.c value.c vm.c" \
	  CC=$(CC_PSOC) AR=$(AR_PSOC)

build_lib: src/mrubyc/src/hal_user_reerved/hal.c
	@echo "building libmrubyc.a ----------"
	cd src/mrubyc/src ; \
	  $(MAKE) clean all CFLAGS="$(CFLAGS) -DMRBC_CONVERT_CRLF" LDFLAGS="$(LDFLAGS)" HAL_DIR="$(HAL_DIR)" \
	  CC=$(CC) AR=$(AR)
	mv src/mrubyc/src/*.o $(LIB_DIR)/
	mv src/mrubyc/src/libmrubyc.a $(LIB_DIR)/libmrubyc.a
	@echo "building libmmrbc.a ----------"
	cd src ; \
	  $(MAKE) clean all CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
	  CC=$(CC) AR=$(AR)
	mv src/*.o $(LIB_DIR)/
	mv src/ruby-lemon-parse/*.o $(LIB_DIR)/
	mv src/libmmrbc.a $(LIB_DIR)/libmmrbc.a

src/mrubyc/src/hal_user_reerved/hal.c:
	cd src/mrubyc/src/hal_user_reserved/ ;\
	  ln -s ../../../../cli/mmirb_lib/hal_posix/hal.c ./hal.c ; \
	  ln -s ../../../../cli/mmirb_lib/hal_posix/hal.h ./hal.h

build_bin:
	@echo "building mmrbc mmruby mmirb----------"
	cd cli ; \
	  $(MAKE) all CFLAGS="$(CFLAGS)" \
	  LDFLAGS="$(LDFLAGS)" LIB_DIR=$(LIB_DIR) \
	  CC=$(CC) AR=$(AR)
	mv cli/mmruby $(BIN_DIR)/mmruby
	mv cli/mmrbc $(BIN_DIR)/mmrbc
	mv cli/mmirb $(BIN_DIR)/mmirb

gdb: host_debug
	$(GDB) --args ./build/host-debug/bin/mmrbc $(TEST_FILE)

irb: host_debug
	@cd bin ; bundle exec ruby mmirb.rb

clean:
	cd src ; $(MAKE) clean
	cd cli ; $(MAKE) clean
	rm -f $(LIB_DIR_HOST_DEBUG)/*.o
	rm -f $(LIB_DIR_HOST_PRODUCTION)/*.o
	rm -f $(LIB_DIR_HOST_DEBUG)/*.a
	rm -f $(LIB_DIR_HOST_PRODUCTION)/*.a
	rm -f $(BIN_DIR_HOST_DEBUG)/*
	rm -f $(BIN_DIR_HOST_PRODUCTION)/*
	rm -f $(LIB_DIR_ARM_DEBUG)/*.o
	rm -f $(LIB_DIR_ARM_PRODUCTION)/*.o
	rm -f $(LIB_DIR_ARM_DEBUG)/*.a
	rm -f $(LIB_DIR_ARM_PRODUCTION)/*.a
	rm -f $(BIN_DIR_ARM_DEBUG)/*
	rm -f $(BIN_DIR_ARM_PRODUCTION)/*
	rm -f $(LIB_DIR_PSOC5LP)/*.o
	rm -f $(LIB_DIR_PSOC5LP)/*.a

install:
	cp cli/mmrbc /usr/local/bin/mmrbc
	cp cli/mmruby /usr/local/bin/mmruby
