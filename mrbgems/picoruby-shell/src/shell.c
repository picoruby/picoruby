#include <mruby_compiler.h>
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

static void
c_mrbc(struct VM *vm, mrbc_value v[], int argc)
{
  mrc_ccontext *c = mrc_ccontext_new(NULL);
  uint8_t *script = GET_STRING_ARG(1);
  size_t size = strlen((const char *)script);
  mrc_irep *irep = mrc_load_string_cxt(c, (const uint8_t **)&script, size);
  if (!irep) {
    mrc_ccontext_free(c);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Compile failed");
    return;
  }
  uint8_t flags = 0;
  uint8_t *bin = NULL;
  size_t bin_size = 0;
  int result = mrc_dump_irep(c, (const mrc_irep *)irep, flags, &bin, &bin_size);
  mrc_irep_free(c, irep);
  if (result != MRC_DUMP_OK) {
    mrc_ccontext_free(c);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Dump failed");
    return;
  }
  mrbc_value mrb = mrbc_string_new(vm, bin, bin_size);
  mrbc_free(vm, bin);
  SET_RETURN(mrb);
  mrc_ccontext_free(c);
}

void
mrbc_shell_init(void)
{
  mrbc_class *mrbc_class_PicoRubyVM = mrbc_define_class(0, "PicoRubyVM", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_PicoRubyVM, "memory_statistics", c_memory_statistics);
  mrbc_define_method(0, mrbc_class_object, "mrbc", c_mrbc);
}

