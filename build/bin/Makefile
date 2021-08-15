

host_all: host_debug host_production

host_debug: host_debug_libc host_debug_mrbc

host_production: host_production_libc host_production_mrbc

host_production_libc:
	@mkdir -p $(BIN_DIR_HOST_PRODUCTION_LIBC)
	$(MAKE) picorbc BIN_DIR="$(BIN_DIR_HOST_PRODUCTION_LIBC)" \
	  CFLAGS="$(CFLAGS) -DMRBC_ALLOC_LIBC" LDFLAGS="$(LDFLAGS)" CC=$(CC) AR=$(AR)

picorbc:
	@echo
	@echo "================================"
	@echo "building picorbc"
	@echo "  BIN_DIR=$(BIN_DIR)"
	cd ../cli ; \
	  $(MAKE) picorbc CFLAGS="$(CFLAGS)" \
	  LDFLAGS="$(LDFLAGS)" LIB_DIR=$(LIB_DIR) \
	  CC=$(CC) AR=$(AR)
	mv cli/picorbc $(BIN_DIR)/picorbc
