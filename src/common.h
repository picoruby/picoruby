#ifndef MMRBC_COMMON_H_
#define MMRBC_COMMON_H_

#define MAX_LINE_LENGTH 256

#define MAX_TOKEN_LENGTH 256

void *mmrbc_alloc(size_t size);

void mmrbc_free(void *ptr);

char *strsafecpy(char *str1, const char *str2, size_t max);

char *strsafencpy(char *s1, const char *s2, size_t n, size_t max);

char *strsafecat(char *dst, const char *src, size_t max);

#endif
