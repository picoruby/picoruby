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
  SET_RETURN(ret);
#else
  mrbc_raise(vm, MRBC_CLASS(RuntimeError), "memory_statistics is not supported in MRBC_ALLOC_LIBC");
#endif /* MRBC_ALLOC_LIBC */
}

static void
c_start_alloc_profiling(struct VM *vm, mrbc_value v[], int argc)
{
#ifndef MRBC_ALLOC_LIBC
  mrbc_start_alloc_profiling();
#endif /* MRBC_ALLOC_LIBC */
  SET_INT_RETURN(0);
}

static void
c_stop_alloc_profiling(struct VM *vm, mrbc_value v[], int argc)
{
#ifndef MRBC_ALLOC_LIBC
  mrbc_stop_alloc_profiling();
#endif /* MRBC_ALLOC_LIBC */
  SET_INT_RETURN(0);
}

static void
c_get_alloc_peak(struct VM *vm, mrbc_value v[], int argc)
{
#ifndef MRBC_ALLOC_LIBC
  mrbc_value ret = mrbc_hash_new(vm, 2);
  struct MRBC_ALLOC_PROF *prof = mrbc_get_alloc_profiling();
  mrbc_hash_set(&ret,
    &mrbc_symbol_value(mrbc_str_to_symid("peak")),
    &mrbc_integer_value(prof->max - prof->initial)
  );
  mrbc_hash_set(&ret,
    &mrbc_symbol_value(mrbc_str_to_symid("valley")),
    &mrbc_integer_value(prof->min - prof->initial)
  );
  SET_RETURN(ret);
#else
  mrbc_raise(vm, MRBC_CLASS(RuntimeError), "profile_alloc is not supported in MRBC_ALLOC_LIBC");
#endif /* MRBC_ALLOC_LIBC */
}

void
mrbc_picorubyvm_init(void)
{
  mrbc_class *mrbc_class_PicoRubyVM = mrbc_define_class(0, "PicoRubyVM", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_PicoRubyVM, "memory_statistics", c_memory_statistics);
  mrbc_define_method(0, mrbc_class_PicoRubyVM, "start_alloc_profiling", c_start_alloc_profiling);
  mrbc_define_method(0, mrbc_class_PicoRubyVM, "stop_alloc_profiling", c_stop_alloc_profiling);
  mrbc_define_method(0, mrbc_class_PicoRubyVM, "get_alloc_peak", c_get_alloc_peak);
}
