#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "../include/picorb_heap.h"

#if defined(PICORB_ALLOC_ESTALLOC)

static ESTALLOC *picorb_heap_estalloc = NULL;
static uintptr_t picorb_heap_start = 0;
static uintptr_t picorb_heap_end = 0;

int
picorb_heap_init(void *heap, size_t size)
{
  if (heap == NULL || size == 0 || size > UINT_MAX) {
    picorb_heap_estalloc = NULL;
    picorb_heap_start = 0;
    picorb_heap_end = 0;
    return -1;
  }
  picorb_heap_estalloc = est_init(heap, (unsigned int)size);
  if (picorb_heap_estalloc == NULL) {
    picorb_heap_start = 0;
    picorb_heap_end = 0;
    return -1;
  }
  picorb_heap_start = (uintptr_t)heap;
  picorb_heap_end = picorb_heap_start + size;
  return 0;
}

bool
picorb_heap_ready(void)
{
  return picorb_heap_estalloc != NULL;
}

bool
picorb_heap_owns(const void *ptr)
{
  uintptr_t addr = (uintptr_t)ptr;
  return picorb_heap_estalloc != NULL &&
         picorb_heap_start <= addr &&
         addr < picorb_heap_end;
}

void *
picorb_heap_malloc(size_t size)
{
  if (picorb_heap_estalloc == NULL || size > UINT_MAX) {
    return NULL;
  }
  return est_malloc(picorb_heap_estalloc, (unsigned int)size);
}

void *
picorb_heap_calloc(size_t nmemb, size_t size)
{
  if (picorb_heap_estalloc == NULL) {
    return NULL;
  }
  if (size != 0 && nmemb > SIZE_MAX / size) {
    return NULL;
  }
  if (nmemb > UINT_MAX || size > UINT_MAX) {
    return NULL;
  }
  return est_calloc(picorb_heap_estalloc, (unsigned int)nmemb, (unsigned int)size);
}

void *
picorb_heap_realloc(void *ptr, size_t size)
{
  if (ptr == NULL) {
    return picorb_heap_malloc(size);
  }
  if (size == 0) {
    picorb_heap_free(ptr);
    return NULL;
  }
  if (picorb_heap_estalloc == NULL || size > UINT_MAX) {
    return NULL;
  }
  return est_realloc(picorb_heap_estalloc, ptr, (unsigned int)size);
}

void
picorb_heap_free(void *ptr)
{
  if (picorb_heap_estalloc == NULL || ptr == NULL) {
    return;
  }
  est_free(picorb_heap_estalloc, ptr);
}

size_t
picorb_heap_usable_size(void *ptr)
{
  if (picorb_heap_estalloc == NULL || ptr == NULL) {
    return 0;
  }
  return (size_t)est_usable_size(picorb_heap_estalloc, ptr);
}

void
picorb_heap_set_critical_section(void (*enter)(void), void (*exit_func)(void))
{
  if (picorb_heap_estalloc) {
    est_set_critical_section(picorb_heap_estalloc, enter, exit_func);
  }
}

int
picorb_heap_stat(ESTALLOC_STAT *stat)
{
  if (picorb_heap_estalloc == NULL || stat == NULL) {
    return -1;
  }
  est_take_statistics(picorb_heap_estalloc);
  memcpy(stat, &picorb_heap_estalloc->stat, sizeof(*stat));
  return 0;
}

#endif
