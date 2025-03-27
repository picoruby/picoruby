#include <string.h>

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/hash.h"

#if defined(PICORB_ALLOC_TLSF)

#include "../lib/tlsf/tlsf.h"

#if defined(PICORUBY_DEBUG)
static size_t peak_usage = 0;
static size_t current_usage = 0;
#endif

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
mrb_tlsf_allocf(mrb_state *mrb, void *p, size_t size, void *tlsf)
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
mrb_alloc_statistics(mrb_state *mrb)
{
  struct walker_data data = { 0, 0, 0, 0, NULL };
  tlsf_t tlsf = (tlsf_t)mrb->allocf_ud;
  tlsf_walk_pool(tlsf_get_pool(tlsf), mrb_tlsf_walker, &data);
#if defined(PICORUBY_DEBUG)
  mrb_value hash = mrb_hash_new_capa(mrb, 6);
#else
  mrb_value hash = mrb_hash_new_capa(mrb, 5);
#endif
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(allocator)), mrb_symbol_value(MRB_SYM(TLSF)));
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
mrb_open_with_custom_alloc(void* mem, size_t bytes)
{
  tlsf_t tlsf = tlsf_create_with_pool(mem, bytes);
  return mrb_open_allocf(mrb_tlsf_allocf, &tlsf);
}

#elif defined(PICORB_ALLOC_O1HEAP)

#include "../lib/o1heap/o1heap/o1heap.h"

typedef struct Fragment Fragment;

typedef struct FragmentHeader
{
    Fragment* next;
    Fragment* prev;
    size_t    size;
    bool      used;
} FragmentHeader;

struct Fragment
{
    FragmentHeader header;
    // Everything past the header may spill over into the allocatable space. The header survives across alloc/free.
    Fragment* next_free;  // Next free fragment in the bin; NULL in the last one.
    Fragment* prev_free;  // Same but points back; NULL in the first one.
};

static size_t
o1heap_usable_size(const void* const pointer)
{
  if (pointer == NULL) {
      return 0;
  }
  const Fragment* const frag = (const Fragment*) (void*) (((const char*) pointer) - O1HEAP_ALIGNMENT);
  mrb_assert(frag->header.used);
  mrb_assert(frag->header.size >= FRAGMENT_SIZE_MIN);
  return frag->header.size - O1HEAP_ALIGNMENT;
}


static void *
o1heap_realloc(O1HeapInstance *o1heap, void *p, const size_t size)
{
  size_t old_size = o1heap_usable_size(p);
  if (size <= old_size) {
    return p;
  }
  void *new_ptr = o1heapAllocate(o1heap, size);
  if (new_ptr == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < old_size; i++) {
    ((char *)new_ptr)[i] = ((char *)p)[i];
  }
  o1heapFree(o1heap, p);
  return new_ptr;
}

static void *
mrb_o1heap_allocf(mrb_state *mrb, void *p, size_t size, void *o1heap)
{
  if (size == 0) {
    /* `free(NULL)` should be no-op */
    o1heapFree(o1heap, p);
    return NULL;
  }
  /* `ralloc(NULL, size)` works as `malloc(size)` */
  if (p == NULL) {
    return o1heapAllocate(o1heap, size);
  }
  return o1heap_realloc(o1heap, p, size);
}

mrb_value
mrb_alloc_statistics(mrb_state *mrb)
{
  O1HeapInstance *o1heap = (O1HeapInstance *)mrb->allocf_ud;
  O1HeapDiagnostics diag = o1heapGetDiagnostics(o1heap);
  mrb_value hash = mrb_hash_new_capa(mrb, 5);
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(allocator)), mrb_symbol_value(MRB_SYM(O1HEAP)));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(capacity)), mrb_fixnum_value(diag.capacity));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(allocated)), mrb_fixnum_value(diag.allocated));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(peak_allocated)), mrb_fixnum_value(diag.peak_allocated));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(peak_request_size)), mrb_fixnum_value(diag.peak_request_size));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(oom_count)), mrb_fixnum_value(diag.oom_count));
  return hash;
}

mrb_state *
mrb_open_with_custom_alloc(void* mem, size_t bytes)
{
  O1HeapInstance *o1heap = o1heapInit(mem, bytes);
  return mrb_open_allocf(mrb_o1heap_allocf, o1heap);
}

// PICORB_ALLOC_O1HEAP
#elif defined(PICORB_ALLOC_TINYALLOC)

#include "../lib/tinyalloc/tinyalloc.h"

static void *
mrb_ta_allocf(mrb_state *mrb, void *p, size_t size, void *ud)
{
  if (size == 0) {
    /* `free(NULL)` should be no-op */
    if (p != NULL) {
      ta_free(p);
    }
    return NULL;
  }
  if (p == NULL) {
    return ta_alloc(size);
  }
  return ta_realloc(p, size);
}

mrb_value
mrb_alloc_statistics(mrb_state *mrb)
{
  mrb_value hash = mrb_hash_new_capa(mrb, 5);
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(allocator)), mrb_symbol_value(MRB_SYM(TINYALLOC)));
  struct walker_data data = { 0, 0, 0, 0 };
  ta_walk_pool(&data);
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(total)), mrb_fixnum_value(data.total));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(used)), mrb_fixnum_value(data.used));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(free)), mrb_fixnum_value(data.free));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(fresh_blocks)), mrb_fixnum_value(data.fresh_blocks));
  return hash;
}

mrb_state *
mrb_open_with_custom_alloc(void* mem, size_t bytes)
{
  void *limit = (void *)((size_t)mem + bytes -1);
  ta_init(mem, limit, 3000, 16, POOL_ALIGNMENT);
  return mrb_open_allocf(mrb_ta_allocf, NULL);
}

// PICORB_ALLOC_TINYALLOC
#elif defined(PICORB_ALLOC_ESTALLOC)

#include "../lib/estalloc/estalloc.h"

static void *
mrb_estalloc_allocf(mrb_state *mrb, void *p, size_t size, void *est)
{
  if (size == 0) {
    /* `free(NULL)` should be no-op */
    est_free(est, p);
    return NULL;
  }
  /* `ralloc(NULL, size)` works as `malloc(size)` */
  return est_realloc(est, p, size);
}

mrb_value
mrb_alloc_statistics(mrb_state *mrb)
{
  ESTALLOC *est = (ESTALLOC *)mrb->allocf_ud;
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
mrb_open_with_custom_alloc(void* mem, size_t bytes)
{
  ESTALLOC *est = est_init(mem, bytes);
  return mrb_open_allocf(mrb_estalloc_allocf, est);
}

// PICORB_ALLOC_ESTALLOC
#else
// DEFAULT

#include <malloc.h>

#if defined(PICORUBY_DEBUG)

static size_t peak_usage = 0;
static size_t current_usage = 0;

static void *
mrb_libc_allocf(mrb_state *mrb, void *p, size_t size, void *ud)
{
  size_t old_size = malloc_usable_size(p);
  if (size == 0) {
    current_usage -= old_size;
    free(p);
    return NULL;
  }
  void *new_p = realloc(p, size);
  if (new_p == NULL) {
    return NULL;
  }
  peak_usage += malloc_usable_size(new_p);
  current_usage += malloc_usable_size(new_p) - old_size;
  if (peak_usage < current_usage) {
    peak_usage = current_usage;
  }
  return new_p;
}

#else

static void *
mrb_libc_allocf(mrb_state *mrb, void *p, size_t size, void *ud)
{
  if (size == 0) {
    /* `free(NULL)` should be no-op */
    free(p);
    return NULL;
  }
  else {
    /* `ralloc(NULL, size)` works as `malloc(size)` */
    return realloc(p, size);
  }
}

#endif

mrb_value
mrb_alloc_statistics(mrb_state *mrb)
{
  mrb_value hash = mrb_hash_new_capa(mrb, 1);
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(allocator)), mrb_symbol_value(MRB_SYM(DEFAULT)));
#if defined(PICORUBY_DEBUG)
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(peak)), mrb_fixnum_value(peak_usage));
  mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(current)), mrb_fixnum_value(current_usage));
#endif
  return hash;
}

mrb_state *
mrb_open_with_custom_alloc(void* mem, size_t bytes)
{
  (void)mem;
  (void)bytes;
  return mrb_open_allocf(mrb_libc_allocf, NULL);
}

#endif
