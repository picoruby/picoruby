#include <picorbc.h>
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

  StreamInterface *si = StreamInterface_new(NULL, GET_STRING_ARG(1), STREAM_TYPE_MEMORY);
  ParserState *p = Compiler_parseInitState(0, si->node_box_size);
  if (Compiler_compile(p, si, NULL)) {
    mrbc_value mrb = mrbc_string_new(vm, p->scope->vm_code, p->scope->vm_code_size);
    SET_RETURN(mrb);
  } else {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Compile failed");
  }
  StreamInterface_free(si);
  Compiler_parserStateFree(p);
}

void
mrbc_shell_init(void)
{
  mrbc_define_method(0, mrbc_class_object, "memory_statistics", c_memory_statistics);
  mrbc_define_method(0, mrbc_class_object, "mrbc", c_mrbc);

  mrbc_sym sym_id = mrbc_str_to_symid("SIZEOF_POINTER");
  mrbc_set_const(sym_id, &mrbc_integer_value(PICORBC_PTR_SIZE));
}

