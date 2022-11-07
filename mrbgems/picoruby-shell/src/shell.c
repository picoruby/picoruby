#include <mrubyc.h>

static void
c_memory_statistics(struct VM *vm, mrbc_value v[], int argc)
{
#ifndef MRBC_ALLOC_LIBC
  struct MRBC_ALLOC_STATISTICS mem;
  mrbc_alloc_statistics(&mem);
  mrbc_value ret = mrbc_hash_new(vm, 4);
  mrbc_hash_set(
    &ret,
    &mrbc_symbol_value(mrbc_str_to_symid("total")),
    &mrbc_integer_value(mem.total)
  );
  mrbc_hash_set(
    &ret,
    &mrbc_symbol_value(mrbc_str_to_symid("used")),
    &mrbc_integer_value(mem.used)
  );
  mrbc_hash_set(
    &ret,
    &mrbc_symbol_value(mrbc_str_to_symid("free")),
    &mrbc_integer_value(mem.free)
  );
  mrbc_hash_set(
    &ret,
    &mrbc_symbol_value(mrbc_str_to_symid("frag")),
    &mrbc_integer_value(mem.fragmentation)
  );
#else
  mrbc_value ret = mrbc_hash_new(vm, 4);
  mrbc_value message = mrbc_string_new_cstr(vm, "memory_statistics doesn't work");
  mrbc_hash_set(
    &ret,
    &mrbc_symbol_value(mrbc_str_to_symid("error")),
    &message
  );
#endif /* MRBC_ALLOC_LIBC */
  SET_RETURN(ret);
}

void
mrbc_shell_init(void)
{
  mrbc_define_method(0, mrbc_class_object, "memory_statistics", c_memory_statistics);
}

