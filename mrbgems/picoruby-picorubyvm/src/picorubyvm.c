#include <mrubyc.h>
#include "../include/instruction_sequence.h"

static void
c_memory_statistics(struct VM *vm, mrbc_value v[], int argc)
{
#if !defined(MRBC_ALLOC_LIBC)
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
#if !defined(MRBC_ALLOC_LIBC) && defined(MRBC_USE_ALLOC_PROF)
  mrbc_start_alloc_profiling();
#endif
  SET_INT_RETURN(0);
}

static void
c_stop_alloc_profiling(struct VM *vm, mrbc_value v[], int argc)
{
#if !defined(MRBC_ALLOC_LIBC) && defined(MRBC_USE_ALLOC_PROF)
  mrbc_stop_alloc_profiling();
#endif
  SET_INT_RETURN(0);
}

static void
c_alloc_profiling_result(struct VM *vm, mrbc_value v[], int argc)
{
#if !defined(MRBC_ALLOC_LIBC) && defined(MRBC_USE_ALLOC_PROF)
  mrbc_value ret = mrbc_hash_new(vm, 2);
  struct MRBC_ALLOC_PROF prof;
  mrbc_get_alloc_profiling(&prof);
  mrbc_hash_set(&ret,
    &mrbc_symbol_value(mrbc_str_to_symid("peak")),
    &mrbc_integer_value(prof.max - prof.initial)
  );
  mrbc_hash_set(&ret,
    &mrbc_symbol_value(mrbc_str_to_symid("valley")),
    &mrbc_integer_value(prof.min - prof.initial)
  );
  SET_RETURN(ret);
#else
  mrbc_raise(vm, MRBC_CLASS(RuntimeError), "profile_alloc is not supported in MRBC_ALLOC_LIBC");
#endif
}

void
mrbc_picorubyvm_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_PicoRubyVM = mrbc_define_class(vm, "PicoRubyVM", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_PicoRubyVM, "memory_statistics", c_memory_statistics);
  mrbc_define_method(vm, mrbc_class_PicoRubyVM, "start_alloc_profiling", c_start_alloc_profiling);
  mrbc_define_method(vm, mrbc_class_PicoRubyVM, "stop_alloc_profiling", c_stop_alloc_profiling);
  mrbc_define_method(vm, mrbc_class_PicoRubyVM, "alloc_profiling_result", c_alloc_profiling_result);

  mrbc_instruction_sequence_init(vm);
}

