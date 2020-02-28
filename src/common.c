#include <string.h>
#include "common.h"
#include "debug.h"

#include "mrubyc/src/alloc.h"

int alloc_count = 0;
int free_count = 0;

void *mmrbc_alloc(size_t size)
{
  alloc_count++;
  return malloc(size);
  //return mrbc_raw_alloc(size);
}

void mmrbc_free(void *ptr)
{
  //if (ptr == NULL) return;
  free_count++;
  free(ptr);
  //mrbc_raw_free(ptr);
  ptr = NULL;
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
