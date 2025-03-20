#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/hash.h"
#include "tlsf.h"

#if defined(PICORUBY_DEBUG)
static size_t peak_usage = 0;
static size_t current_usage = 0;
#endif

static tlsf_t tlsf;

struct walker_data {
  size_t total;
  size_t used;
  size_t free;
  size_t fragment;
  void *last_free_block;
};

static void
mrb_tlsf_walker(void *ptr, size_t size, int used, void *user)
{
  struct walker_data *data = (struct walker_data *)user;
  size_t size_with_padding = tlsf_block_size(ptr);
  if (used) {
    data->used += size_with_padding;
  } else if (size > 0) {
    data->free += size;
    // Check if this free block is contiguous with the last one
    if (data->last_free_block == NULL || (char *)data->last_free_block + tlsf_block_size(data->last_free_block) != (char *)ptr) {
      data->fragment++;
    }
    data->last_free_block = ptr;
  }
  data->total += size_with_padding;
}

static void *
mrb_tlsf_allocf(mrb_state *mrb, void *p, size_t size, void *ud)
{
#if defined(PICORUBY_DEBUG)
  if (size == 0) {
    current_usage -= tlsf_block_size(p);
    tlsf_free(tlsf, p);
    return NULL;
  }
  else {
    size_t old_size = p ? tlsf_block_size(p) : 0;
    void *new_ptr = tlsf_realloc(tlsf, p, size);
    size_t new_size = tlsf_block_size(new_ptr);
    current_usage += (new_size - old_size);
    if (peak_usage < current_usage) {
      peak_usage = current_usage;
    }
    return new_ptr;
  }
#else
  if (size == 0) {
    /* `free(NULL)` should be no-op */
    tlsf_free(tlsf, p);
    return NULL;
  }
  else {
    /* `ralloc(NULL, size)` works as `malloc(size)` */
    return tlsf_realloc(tlsf, p, size);
  }
#endif
}

mrb_value
mrb_tlsf_statistics(mrb_state *mrb)
{
  struct walker_data data = { 0, 0, 0, 0 };
  tlsf_walk_pool(tlsf_get_pool(tlsf), mrb_tlsf_walker, &data);
#if defined(PICORUBY_DEBUG)
  mrb_value hash = mrb_hash_new_capa(mrb, 5);
#else
  mrb_value hash = mrb_hash_new_capa(mrb, 4);
#endif
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(total)), mrb_fixnum_value(data.total));
#if defined(PICORUBY_DEBUG)
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(peak)), mrb_fixnum_value(peak_usage));
#endif
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(used)), mrb_fixnum_value(data.used));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(free)), mrb_fixnum_value(data.free));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(fragment)), mrb_fixnum_value(data.fragment));
  return hash;
}

mrb_state *
mrb_open_with_tlsf(void* mem, size_t bytes)
{
  tlsf = tlsf_create_with_pool(mem, bytes);
  return mrb_open_allocf(mrb_tlsf_allocf, NULL);
}
