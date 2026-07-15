#include <limits.h>

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/hash.h"
#include "../../lib/estalloc/estalloc.h"

static ESTALLOC *est = NULL;

void *
mrb_basic_alloc_func(void *ptr, size_t size)
{
  if (size == 0) {
    est_free(est, ptr);
    return NULL;
  }
  if (size > UINT_MAX) {
    return NULL;
  }
  return est_realloc(est, ptr, (unsigned int)size);
}

mrb_value
mrb_alloc_statistics(mrb_state *mrb)
{
  est_take_statistics(est);
  ESTALLOC_STAT *stat = &est->stat;
  mrb_value hash = mrb_hash_new_capa(mrb, 5);
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(allocator)), mrb_symbol_value(MRB_SYM(ESTALLOC)));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(total)), mrb_fixnum_value(stat->total));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(used)), mrb_fixnum_value(stat->used));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(free)), mrb_fixnum_value(stat->free));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(frag)), mrb_fixnum_value(stat->frag));
  return hash;
}

mrb_state *
mrb_open_with_custom_alloc(void *mem, size_t bytes)
{
  if (bytes > UINT_MAX) {
    return NULL;
  }
  est = est_init(mem, (unsigned int)bytes);
  return mrb_open();
}

void
mrb_alloc_set_critical_section(void (*enter)(void), void (*exit_func)(void))
{
  if (est) {
    est_set_critical_section(est, enter, exit_func);
  }
}
