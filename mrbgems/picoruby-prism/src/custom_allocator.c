#include "alloc.h"
#include <string.h>

void*
picorb_alloc(unsigned int size)
{
  return mrbc_raw_alloc(size);
}

void
picorb_free(void* ptr)
{
  mrbc_raw_free(ptr);
}

void*
picorb_calloc(unsigned int nmemb, unsigned int size)
{
  unsigned int total_size = nmemb * size;
  void* ptr = mrbc_raw_alloc(total_size);
  if (ptr != NULL) {
    memset(ptr, 0, total_size);
  }
  return ptr;
}

void*
picorb_realloc(void *ptr, unsigned int size)
{
  if (ptr == NULL) {
    return mrbc_raw_alloc(size);
  } else {
    return mrbc_raw_realloc(ptr, size);
  }
}

