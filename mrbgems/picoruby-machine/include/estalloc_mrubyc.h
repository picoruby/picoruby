#ifndef PICORUBY_ESTALLOC_MRUBYC_H
#define PICORUBY_ESTALLOC_MRUBYC_H

#include <stddef.h>
#include "picorb_heap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PICORB_ESTALLOC_MRUBYC_INIT(heap, size) picorb_estalloc_mrubyc_init((heap), (size))

void picorb_estalloc_mrubyc_init(void *heap, size_t size);
int picorb_estalloc_mrubyc_statistics(ESTALLOC_STAT *stat);

#ifdef __cplusplus
}
#endif

#endif
