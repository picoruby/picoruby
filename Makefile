MAKEFILE_DIR := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
GDB := gdb
CFLAGS += -Wall -Wpointer-arith -std=gnu99 -DHEAP_SIZE=1000000 -DMAX_REGS_SIZE=256 -DMAX_SYMBOLS_COUNT=500
LDFLAGS +=
TEST_FILE := test/fixtures/hello_world.rb
DEPS := cli/heap.h cli/picoshell.c cli/picoruby.c cli/picorbc.c cli/picoirb.c \
        cli/picoshell_lib/shell.rb \
TARGETS = $(BIN_DIR)/picorbc $(BIN_DIR)/picoruby $(BIN_DIR)/picoshell $(BIN_DIR)/picoirb

BIN_DIR_HOST_DEBUG_LIBC      := build/bin/host-debug/alloc_libc
BIN_DIR_HOST_PRODUCTION_LIBC := build/bin/host-production/alloc_libc
BIN_DIR_HOST_DEBUG_MRBC      := build/bin/host-debug/alloc_mrbc
BIN_DIR_HOST_PRODUCTION_MRBC := build/bin/host-production/alloc_mrbc
BIN_DIR_ARM_DEBUG_LIBC       := build/bin/arm-debug/alloc_libc
BIN_DIR_ARM_PRODUCTION_LIBC  := build/bin/arm-production/alloc_libc
BIN_DIR_ARM_DEBUG_MRBC       := build/bin/arm-debug/alloc_mrbc
BIN_DIR_ARM_PRODUCTION_MRBC  := build/bin/arm-production/alloc_mrbc

development: host_debug_mrbc

gdb: host_debug_mrbc
	$(GDB) --args $(BIN_DIR_HOST_DEBUG_MRBC)/picorbc $(TEST_FILE)

all: host_all arm_all

host_all: host_debug host_production
host_debug: host_debug_libc host_debug_mrbc
host_production: host_production_libc host_production_mrbc

arm_all: arm_debug arm_production
arm_debug: arm_debug_libc arm_debug_mrbc
arm_production: arm_production_libc arm_production_mrbc

host_debug_libc: src/mrubyc/src/mrblib.c
	@mkdir -p $(BIN_DIR_HOST_DEBUG_LIBC)
	cd build/lib ; $(MAKE) host_debug_libc \
	  LIB="libpicorbc libmrubyc" \
	  CFLAGS="-O0 -g3 $(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picorbc DIR=host-debug/alloc_libc \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" \
	  LIBS="-lpicorbc"
	cd cli ; $(MAKE) picoruby picoirb DIR=host-debug/alloc_libc \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" \
	  MRBC=$(MAKEFILE_DIR)$(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc \
	  LIBS="-lmrubyc -lpicorbc"
	@echo "\e[32;1mYey! $@\e[m\n"

host_debug_mrbc: src/mrubyc/src/mrblib.c
	@mkdir -p $(BIN_DIR_HOST_DEBUG_MRBC)
	cd build/lib ; $(MAKE) host_debug_mrbc \
	  LIB="libpicorbc libmrubyc" \
	  CFLAGS="-O0 -g3 $(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picoruby picoirb DIR=host-debug/alloc_mrbc \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" LDFLAGS="$(LDFLAGS)" \
	  MRBC=$(MAKEFILE_DIR)$(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc \
	  LIBS="-lmrubyc -lpicorbc"
	@echo "\e[32;1mYey! $@\e[m\n"

host_production_libc: src/mrubyc/src/mrblib.c
	@mkdir -p $(BIN_DIR_HOST_PRODUCTION_LIBC)
	cd build/lib ; $(MAKE) host_production_libc \
	  LIB="libpicorbc libmrubyc" \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DNDEBUG" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picorbc DIR=host-production/alloc_libc \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DNDEBUG -DMRBC_USE_HAL_USER_RESERVED -DMRBC_ALLOC_LIBC" \
	  LDFLAGS="$(LDFLAGS)" \
	  LIBS="-lpicorbc"
	cd cli ; $(MAKE) picoruby picoirb DIR=host-production/alloc_libc \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DNDEBUG -DMRBC_USE_HAL_USER_RESERVED -DMRBC_ALLOC_LIBC" \
	  LDFLAGS="$(LDFLAGS)" \
	  MRBC=$(MAKEFILE_DIR)$(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc \
	  LIBS="-lpicorbc -lmrubyc"
	@echo "\e[32;1mYey! $@\e[m\n"

host_production_mrbc: src/mrubyc/src/mrblib.c
	@mkdir -p $(BIN_DIR_HOST_PRODUCTION_MRBC)
	cd build/lib ; $(MAKE) host_production_mrbc \
	  LIB="libpicorbc libmrubyc" \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DNDEBUG" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picoruby picoirb DIR=host-production/alloc_mrbc \
	  CFLAGS="-O0 -g3 $(CFLAGS) -DNDEBUG -DMRBC_USE_HAL_USER_RESERVED" LDFLAGS="$(LDFLAGS)" \
	  MRBC=$(MAKEFILE_DIR)$(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc \
	  LIBS="-lmrubyc -lpicorbc"
	@echo "\e[32;1mYey! $@\e[m\n"

CC_ARM := arm-linux-gnueabihf-gcc
AR_ARM := arm-linux-gnueabihf-ar

arm_debug_libc: src/mrubyc/src/mrblib.c
	@mkdir -p $(BIN_DIR_ARM_DEBUG_LIBC)
	cd build/lib ; $(MAKE) arm_debug_libc \
	  LIB="libpicorbc libmrubyc" \
	  CFLAGS="-static -O0 -g3 $(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picorbc DIR=arm-debug/alloc_libc \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" \
	  CC="$(CC_ARM)" AR="$(AR_ARM)" \
	  LIBS="-lpicorbc"
	cd cli ; $(MAKE) picoruby picoirb DIR=arm-debug/alloc_libc \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" \
	  CC="$(CC_ARM)" AR="$(AR_ARM)" \
	  LIBS="-lmrubyc -lpicorbc"
	@echo "\e[32;1mYey! $@\e[m\n"

arm_debug_mrbc: src/mrubyc/src/mrblib.c
	@mkdir -p $(BIN_DIR_ARM_DEBUG_MRBC)
	cd build/lib ; $(MAKE) arm_debug_mrbc \
	  LIB="libpicorbc libmrubyc" \
	  CFLAGS="-static -O0 -g3 $(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picoruby picoirb DIR=arm-debug/alloc_mrbc \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" LDFLAGS="$(LDFLAGS)" \
	  CC="$(CC_ARM)" AR="$(AR_ARM)" \
	  LIBS="-lmrubyc -lpicorbc"
	@echo "\e[32;1mYey! $@\e[m\n"

arm_production_libc: src/mrubyc/src/mrblib.c
	@mkdir -p $(BIN_DIR_ARM_PRODUCTION_LIBC)
	cd build/lib ; $(MAKE) arm_production_libc \
	  LIB="libpicorbc libmrubyc" \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DNDEBUG" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picorbc DIR=arm-production/alloc_libc \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DNDEBUG -DMRBC_USE_HAL_USER_RESERVED -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" \
	  CC="$(CC_ARM)" AR="$(AR_ARM)" \
	  LIBS="-lpicorbc"
	cd cli ; $(MAKE) picoruby picoirb DIR=arm-production/alloc_libc \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DNDEBUG -DMRBC_USE_HAL_USER_RESERVED -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" \
	  CC="$(CC_ARM)" AR="$(AR_ARM)" \
	  LIBS="-lpicorbc -lmrubyc"
	@echo "\e[32;1mYey! $@\e[m\n"

arm_production_mrbc: src/mrubyc/src/mrblib.c
	@mkdir -p $(BIN_DIR_ARM_PRODUCTION_MRBC)
	cd build/lib ; $(MAKE) arm_production_mrbc \
	  LIB="libpicorbc libmrubyc" \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DNDEBUG" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picoruby picoirb DIR=arm-production/alloc_mrbc \
	  CFLAGS="-static -O0 -g3 $(CFLAGS) -DNDEBUG -DMRBC_USE_HAL_USER_RESERVED" LDFLAGS="$(LDFLAGS)" \
	  CC="$(CC_ARM)" AR="$(AR_ARM)" \
	  LIBS="-lmrubyc -lpicorbc"
	@echo "\e[32;1mYey! $@\e[m\n"

host_valgrind:
	rm massif.out.* ; $(MAKE) clean ; $(MAKE) host_production_libc && \
	  valgrind --tool=massif --stacks=yes $(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc test/fixtures/hello_world.rb
	  valgrind --tool=massif --stacks=yes $(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc test/fixtures/larger_script.rb

psoc5lp_lib:
	cd cli ; $(MAKE) picoshell_lib/shell.c \
	  PICORBC=$(MAKEFILE_DIR)$(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc
	docker-compose up

docker_psoc5lp_lib: src/mrubyc/src/mrblib.c
	cd build/lib ; $(MAKE) psoc5lp_mrbc \
	  LIB="libpicorbc libmrubyc"
	@echo "\e[32;1mYey! $@\e[m\n"

MRBLIB_DEPS := src/mrubyc/mrblib/array.rb \
               src/mrubyc/mrblib/global.rb \
               src/mrubyc/mrblib/hash.rb \
               src/mrubyc/mrblib/numeric.rb \
               src/mrubyc/mrblib/object.rb \
               src/mrubyc/mrblib/range.rb \
               src/mrubyc/mrblib/string.rb

src/mrubyc/src/mrblib.c: $(MRBLIB_DEPS)
	cd build/lib ; $(MAKE) host_production_libc \
	  LIB="libpicorbc" \
	  CFLAGS="-Os -DNDEBUG -Wl,-s $(CFLAGS)" LDFLAGS="$(LDFLAGS)"
	cd cli ; $(MAKE) picorbc DIR="host-production/alloc_libc" \
	  CFLAGS="-Os -DNDEBUG -Wl,-s $(CFLAGS)" LDFLAGS="$(LDFLAGS)" \
	  LIBS="-lpicorbc"
	@echo
	@echo "================================"
	@echo "building mrblib"
	cd src/mrubyc/mrblib ; \
	  $(MAKE) distclean all MRBC=$(MAKEFILE_DIR)$(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc
	  #$(MAKE) distclean all MRBC=/home/hasumi/.rbenv/shims/mrbc
	@echo "\e[32;1mYey! $@\e[m\n"

test: check

check: host_production_mrbc
	PICORUBY=$(MAKEFILE_DIR)$(BIN_DIR_HOST_PRODUCTION_MRBC)/picoruby ruby ./test/helper/test.rb

picoshell: host_debug_mrbc
	cd cli ; \
	  $(MAKE) picoshell CFLAGS="$(CFLAGS) -DMRBC_USE_HAL_USER_RESERVED" \
	  LDFLAGS="$(LDFLAGS)" LIB_DIR="$(MAKEFILE_DIR)build/lib/host-debug/alloc_mrbc" \
	  PICORBC=$(MAKEFILE_DIR)$(BIN_DIR_HOST_PRODUCTION_LIBC)/picorbc
	mv cli/picoshell $(BIN_DIR_HOST_DEBUG_MRBC)/picoshell
	@which socat || (echo "\nsocat is not installed\nPlease install socat\n"; exit 1)
	@which cu || (echo "\ncu is not installed\nPlease install cu\n"; exit 1)
	@cd bin ; bundle install && bundle exec ruby picoshell.rb
	@echo "\e[32;1mYey! $@\e[m\n"

clean:
	cd src ; $(MAKE) clean
	cd src/mrubyc/mrblib ; $(MAKE) distclean
	cd src/mrubyc ; $(MAKE) clean
	cd cli ; $(MAKE) clean
	cd build/lib ; $(MAKE) clean
	rm -f $(BIN_DIR_HOST_DEBUG_LIBC)/*
	rm -f $(BIN_DIR_HOST_DEBUG_LIBC)/*
	rm -f $(BIN_DIR_HOST_PRODUCTION_LIBC)/*
	rm -f $(BIN_DIR_HOST_PRODUCTION_LIBC)/*
	rm -f $(BIN_DIR_HOST_DEBUG_MRBC)/*
	rm -f $(BIN_DIR_HOST_DEBUG_MRBC)/*
	rm -f $(BIN_DIR_HOST_PRODUCTION_MRBC)/*
	rm -f $(BIN_DIR_HOST_PRODUCTION_MRBC)/*
	rm -f $(BIN_DIR_ARM_DEBUG_LIBC)/*
	rm -f $(BIN_DIR_ARM_DEBUG_LIBC)/*
	rm -f $(BIN_DIR_ARM_PRODUCTION_LIBC)/*
	rm -f $(BIN_DIR_ARM_PRODUCTION_LIBC)/*
	rm -f $(BIN_DIR_ARM_DEBUG_MRBC)/*
	rm -f $(BIN_DIR_ARM_DEBUG_MRBC)/*
	rm -f $(BIN_DIR_ARM_PRODUCTION_MRBC)/*
	rm -f $(BIN_DIR_ARM_PRODUCTION_MRBC)/*

install:
	cp cli/picorbc /usr/local/bin/picorbc
	cp cli/picoruby /usr/local/bin/picoruby

guard:
	if [ ! -f Gemfile.lock ]; then bundle install; fi;
	bundle exec guard start
