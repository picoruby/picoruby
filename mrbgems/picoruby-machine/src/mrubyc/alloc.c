#include <stddef.h>
#include "../../include/estalloc_mrubyc.h"

void
picorb_estalloc_mrubyc_init(void *heap, size_t size)
{
  picorb_heap_init(heap, size);
}

int
picorb_estalloc_mrubyc_statistics(ESTALLOC_STAT *stat)
{
  return picorb_heap_stat(stat);
}

#if !defined(PICORB_WRAP_LIBC_ALLOC)
void *
malloc(size_t size)
{
  return picorb_heap_malloc(size);
}

void *
calloc(size_t nmemb, size_t size)
{
  return picorb_heap_calloc(nmemb, size);
}

void *
realloc(void *ptr, size_t size)
{
  return picorb_heap_realloc(ptr, size);
}

void
free(void *ptr)
{
  picorb_heap_free(ptr);
}
#endif
