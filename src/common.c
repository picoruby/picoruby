#include <string.h>
#include "common.h"
#include "debug.h"

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
