#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/hash.h"
#include "../../include/picorb_heap.h"

void *
mrb_basic_alloc_func(void *ptr, size_t size)
{
  if (size == 0) {
    picorb_heap_free(ptr);
    return NULL;
  }
  return picorb_heap_realloc(ptr, size);
}

mrb_value
mrb_alloc_statistics(mrb_state *mrb)
{
  ESTALLOC_STAT mem;
  picorb_heap_stat(&mem);
  mrb_value hash = mrb_hash_new_capa(mrb, 6);
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(allocator)), mrb_symbol_value(MRB_SYM(ESTALLOC)));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(total)), mrb_fixnum_value(mem.total));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(used)), mrb_fixnum_value(mem.used));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(free)), mrb_fixnum_value(mem.free));
  mrb_hash_set(mrb, hash, mrb_symbol_value(mrb_intern_lit(mrb, "max_free")), mrb_fixnum_value(mem.max_free));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(frag)), mrb_fixnum_value(mem.frag));
  return hash;
}

mrb_state *
mrb_open_with_custom_alloc(void *mem, size_t bytes)
{
  if (picorb_heap_init(mem, bytes) != 0) {
    return NULL;
  }
  return mrb_open();
}

void
mrb_alloc_set_critical_section(void (*enter)(void), void (*exit_func)(void))
{
  picorb_heap_set_critical_section(enter, exit_func);
}
