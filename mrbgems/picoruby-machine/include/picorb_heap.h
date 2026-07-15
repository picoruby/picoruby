#ifndef PICORUBY_PICORB_HEAP_H
#define PICORUBY_PICORB_HEAP_H

#include <stdbool.h>
#include <stddef.h>
#include "../lib/estalloc/estalloc.h"

#ifdef __cplusplus
extern "C" {
#endif

int picorb_heap_init(void *heap, size_t size);
bool picorb_heap_ready(void);
bool picorb_heap_owns(const void *ptr);
void *picorb_heap_malloc(size_t size);
void *picorb_heap_calloc(size_t nmemb, size_t size);
void *picorb_heap_realloc(void *ptr, size_t size);
void picorb_heap_free(void *ptr);
size_t picorb_heap_usable_size(void *ptr);
void picorb_heap_set_critical_section(void (*enter)(void), void (*exit_func)(void));
int picorb_heap_stat(ESTALLOC_STAT *stat);

#ifdef __cplusplus
}
#endif

#endif
