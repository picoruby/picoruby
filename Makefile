CC := gcc
CC_ARM := arm-linux-gnueabihf-gcc
CFLAGS += -Wall -Wpointer-arith -std=gnu99
LDFLAGS += -static
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
        src/common.h    src/common.h \
        src/compiler.h  src/compiler.h \
        src/debug.h \
        src/generator.h src/generator.h \
        src/my_regex.h  src/my_regex.h \
        src/node.h      src/node.h \
        src/scope.h     src/scope.h \
        src/stream.h    src/stream.h \
        src/token_data.h \
        src/token.h     src/token.h \
        src/tokenizer.h src/tokenizer.h \
        src/version.h   src/version.h \
        src/ruby-lemon-parse/parse_header.h src/ruby-lemon-parse/parse.y \
        src/ruby-lemon-parse/crc.c
TARGETS = $(BIN_DIR)/mmrbc $(BIN_DIR)/mmruby $(BIN_DIR)/mmirb

default: host_debug

all: host_all arm_all

host_all:
	$(MAKE) host_debug host_production CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

host_debug:
	@mkdir -p $(LIB_DIR_HOST_DEBUG)
	@mkdir -p $(BIN_DIR_HOST_DEBUG)
	$(MAKE) $(BIN_DIR_HOST_DEBUG)/mmirb CC=$(CC) CFLAGS="-O0 -g3 $(CFLAGS)" LDFLAGS="$(LDFLAGS)" LIB_DIR=$(LIB_DIR_HOST_DEBUG) BIN_DIR=$(BIN_DIR_HOST_DEBUG)

host_production:
	@mkdir -p $(LIB_DIR_HOST_PRODUCTION)
	@mkdir -p $(BIN_DIR_HOST_PRODUCTION)
	$(MAKE) $(BIN_DIR_HOST_PRODUCTION)/mmirb CC=$(CC) CFLAGS="-Os -DNDEBUG $(CFLAGS)" LDFLAGS="-Wl,-s $(LDFLAGS)" LIB_DIR=$(LIB_DIR_HOST_PRODUCTION) BIN_DIR=$(BIN_DIR_HOST_PRODUCTION)

arm_all:
	$(MAKE) arm_debug arm_production CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"

arm_debug:
	mkdir -p $(LIB_DIR_ARM_DEBUG)
	mkdir -p $(BIN_DIR_ARM_DEBUG)
	$(MAKE) $(BIN_DIR_ARM_DEBUG)/mmirb CC=$(CC_ARM) CFLAGS="-O0 -g3 $(CFLAGS)" LDFLAGS="$(LDFLAGS)" LIB_DIR=$(LIB_DIR_ARM_DEBUG) BIN_DIR=$(BIN_DIR_ARM_DEBUG)

arm_production:
	mkdir -p $(LIB_DIR_ARM_PRODUCTION)
	mkdir -p $(BIN_DIR_ARM_PRODUCTION)
	$(MAKE) $(BIN_DIR_ARM_PRODUCTION)/mmirb CC=$(CC_ARM) CFLAGS="-Os -DNDEBUG $(CFLAGS)" LDFLAGS="-Wl,-s $(LDFLAGS)" LIB_DIR=$(LIB_DIR_ARM_PRODUCTION) BIN_DIR=$(BIN_DIR_ARM_PRODUCTION)

$(TARGETS): $(DEPS)
	@echo "building libmrubyc.a ----------"
	cd src/mrubyc/src ; $(MAKE) clean all CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	mv src/mrubyc/src/libmrubyc.a $(LIB_DIR)/libmrubyc.a
	@echo "building libmrubyc-irb.a ----------"
	cd src/mrubyc/src ; $(MAKE) clean all CFLAGS="-DMRBC_IRB $(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	mv src/mrubyc/src/libmrubyc.a $(LIB_DIR)/libmrubyc-irb.a
	@echo "building libmmrbc.a ----------"
	cd src ; $(MAKE) clean all CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	mv src/libmmrbc.a $(LIB_DIR)/libmmrbc.a
	@echo "building mmrbc mmruby mmirb----------"
	cd cli ; $(MAKE) all CC=$(CC) CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" LIB_DIR=$(LIB_DIR)
	mv cli/mmruby $(BIN_DIR)/mmruby
	mv cli/mmrbc $(BIN_DIR)/mmrbc
	mv cli/mmirb $(BIN_DIR)/mmirb

gdb: host_debug
	gdb --args ./build/host-debug/bin/mmrbc $(TEST_FILE)

irb: host_debug
	@cd bin ; bundle exec ruby mmirb.rb

clean:
	cd src ; $(MAKE) clean
	cd cli ; $(MAKE) clean
	rm -f $(LIB_DIR_HOST_DEBUG)/*.a
	rm -f $(LIB_DIR_HOST_PRODUCTION)/*.a
	rm -f $(BIN_DIR_HOST_DEBUG)/*
	rm -f $(BIN_DIR_HOST_PRODUCTION)/*
	rm -f $(LIB_DIR_ARM_DEBUG)/*.a
	rm -f $(LIB_DIR_ARM_PRODUCTION)/*.a
	rm -f $(BIN_DIR_ARM_DEBUG)/*
	rm -f $(BIN_DIR_ARM_PRODUCTION)/*

install:
	cp cli/mmrbc /usr/local/bin/mmrbc
	cp cli/mmruby /usr/local/bin/mmruby
