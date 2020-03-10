#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "debug.h"

#include "mrubyc/src/mrubyc.h"
#include "mrubyc/src/alloc.h"

int alloc_count = 0;
int free_count = 0;

void print_memory(void)
{
#ifndef MRBC_ALLOC_LIBC
  int total, used, free, fragment;
  mrbc_alloc_statistics( &total, &used, &free, &fragment );
  WARN("Memory total:%d, used:%d, free:%d, fragment:%d", total, used, free, fragment );
#endif
}

void print_allocs(void)
{
  INFO("alloc_count: %d\n", alloc_count);
  INFO("free_count: %d\n", free_count);
}

void *mmrbc_alloc(size_t size)
{
  void *ptr;
  alloc_count++;
  ptr = MMRBC_ALLOC(size);
  DEBUG("alloc: %p, size: %d", ptr, (int)size);
  return ptr;
}

void mmrbc_free(void *ptr)
{
  DEBUG("free: %p", ptr);
  free_count++;
  MMRBC_FREE(ptr);
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
