#ifndef MMRBC_COMMON_H_
#define MMRBC_COMMON_H_

#include <stdint.h>

#ifdef MRBC_ALLOC_LIBC
  #include <stdlib.h>
  #define MMRBC_ALLOC(size) malloc(size)
  #define MMRBC_FREE(ptr)   free(ptr)
#else
  #define MMRBC_ALLOC(size) mrbc_raw_alloc(size)
  #define MMRBC_FREE(ptr)   mrbc_raw_free(ptr)
#endif /* MRBC_ALLOC_LIBC */

#define MAX_LINE_LENGTH 256

#define MAX_TOKEN_LENGTH 256

void print_memory(void);

void print_allocs(void);

void *mmrbc_alloc(size_t size);

void mmrbc_free(void *ptr);

char *strsafecpy(char *str1, const char *str2, size_t max);

char *strsafencpy(char *s1, const char *s2, size_t n, size_t max);

char *strsafecat(char *dst, const char *src, size_t max);

#endif /* MMRBC_COMMON_H_ */
