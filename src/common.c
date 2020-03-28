#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "debug.h"

#include "mrubyc/src/alloc.h"

#ifdef MMRBC_DEBUG
  /* GLOBAL */
  int alloc_count = 0;
  int free_count = 0;
  AllocList *last_alloc;

  void print_memory(void)
  {
  #ifndef MRBC_ALLOC_LIBC
    int total, used, free, fragment;
    mrbc_alloc_statistics( &total, &used, &free, &fragment );
    INFO("Memory total:%d, used:%d, free:%d, fragment:%d", total, used, free, fragment );
  #endif
  }
  void memcheck(void)
  {
    INFO("---MEMCHECK---\nalloc_count: %d, free_count: %d", alloc_count, free_count);
    AllocList *ah = last_alloc;
    while (ah != NULL) {
      WARN("MemoryLeakDetected! alloc_count: %d, ptr: %p", ah->count, ah->ptr);
      if (ah->prev != NULL) {
        ah = ah->prev;
        free(ah->next);
      } else {
        free(ah);
        break;
      }
    }
  }
#endif /* MMRBC_DEBUG */

void *mmrbc_alloc(size_t size)
{
  void *ptr;
  ptr = MMRBC_ALLOC(size);
  DEBUG("alloc: %p, size: %d", ptr, (int)size);
#ifdef MMRBC_DEBUG
  print_memory();
  alloc_count++;
  AllocList *ah = malloc(sizeof(AllocList));
  ah->count = alloc_count;
  ah->ptr = ptr;
  if (last_alloc != NULL) {
    ah->prev = last_alloc;
    last_alloc->next = ah;
  } else {
    ah->prev = NULL;
  }
  ah->next = NULL;
  last_alloc = ah;
#endif
  return ptr;
}

void mmrbc_free(void *ptr)
{
  if (ptr == NULL) return;
  DEBUG("free: %p", ptr);
  MMRBC_FREE(ptr);
#ifdef MMRBC_DEBUG
  print_memory();
  free_count++;
  AllocList *ah = last_alloc;
  if (ah->ptr == ptr) {
    last_alloc = ah->prev;
    free(ah);
  } else {
    while (ah != NULL) {
      if (ah->ptr == ptr) {
        if (ah->prev != NULL) {
          ah->prev->next = ah->next;
        }
        if (ah->next != NULL) {
          ah->next->prev = ah->prev;
        }
        free(ah);
        break;
      }
      ah = ah->prev;
    }
  }
#endif
}

char *strsafencpy(char *s1, const char *s2, size_t n, size_t max)
{
  DEBUG("s1: `%s`, s2: `%s`, n: %d, max: %d", s1, s2, (int)n, (int)max);
  if (n < max) {
    strncpy(s1, s2, n + 1);
    DEBUG("Copied: %s", s1);
  } else {
    FATAL("Can't copy string!");
  }
  return s1;
}

char *strsafecpy(char *s1, const char *s2, size_t max)
{
  DEBUG("s1: `%s`, s2: `%s`, max: %d", s1, s2, (int)max);
  return strsafencpy(s1, s2, strlen(s2), max);
}

char *strsafecat(char *dst, const char *src, size_t max)
{
  DEBUG("dst: `%s`, src: `%s`, max: %d", dst, src, (int)max);
  size_t lensrc = strlen(src);
  if (strlen(dst) + lensrc < max) {
    strncat(dst, src, lensrc);
  } else {
    FATAL("Can't copy string!");
  }
  return dst;
}
