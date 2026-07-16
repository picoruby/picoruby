#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "picorb_heap.h"

#if defined(PICORB_ALLOC_ESTALLOC) && !defined(R2P2_NO_SHARED_ALLOC)

void *__real_malloc(size_t size);
void *__real_calloc(size_t nmemb, size_t size);
void *__real_realloc(void *ptr, size_t size);
void __real_free(void *ptr);
size_t __real_malloc_usable_size(void *ptr);

void *
__wrap_malloc(size_t size)
{
  if (!picorb_heap_ready()) {
    return __real_malloc(size);
  }
  return picorb_heap_malloc(size);
}

void *
__wrap_calloc(size_t nmemb, size_t size)
{
  if (!picorb_heap_ready()) {
    return __real_calloc(nmemb, size);
  }
  return picorb_heap_calloc(nmemb, size);
}

void *
__wrap_realloc(void *ptr, size_t size)
{
  if (ptr == NULL) {
    return __wrap_malloc(size);
  }

  if (!picorb_heap_ready()) {
    return __real_realloc(ptr, size);
  }

  if (picorb_heap_owns(ptr)) {
    return picorb_heap_realloc(ptr, size);
  }

  return __real_realloc(ptr, size);
}

void
__wrap_free(void *ptr)
{
  if (ptr == NULL) {
    return;
  }

  if (picorb_heap_ready() && picorb_heap_owns(ptr)) {
    picorb_heap_free(ptr);
  }
  else {
    __real_free(ptr);
  }
}

size_t
__wrap_malloc_usable_size(void *ptr)
{
  if (ptr == NULL) {
    return 0;
  }

  if (picorb_heap_ready() && picorb_heap_owns(ptr)) {
    return picorb_heap_usable_size(ptr);
  }

  return __real_malloc_usable_size(ptr);
}

#endif
