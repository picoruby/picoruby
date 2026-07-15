#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "../../include/estalloc_mrubyc.h"

static ESTALLOC *picorb_mrubyc_estalloc = NULL;

void
picorb_estalloc_mrubyc_init(void *heap, size_t size)
{
  picorb_mrubyc_estalloc = est_init(heap, (unsigned int)size);
}

int
picorb_estalloc_mrubyc_statistics(ESTALLOC_STAT *stat)
{
  if (picorb_mrubyc_estalloc == NULL || stat == NULL) {
    return -1;
  }
  est_take_statistics(picorb_mrubyc_estalloc);
  *stat = picorb_mrubyc_estalloc->stat;
  return 0;
}

void *
malloc(size_t size)
{
  if (picorb_mrubyc_estalloc == NULL) {
    return NULL;
  }
  if (size > UINT_MAX) {
    return NULL;
  }
  return est_malloc(picorb_mrubyc_estalloc, (unsigned int)size);
}

void *
calloc(size_t nmemb, size_t size)
{
  if (picorb_mrubyc_estalloc == NULL) {
    return NULL;
  }
  if (size != 0 && nmemb > SIZE_MAX / size) {
    return NULL;
  }
  if (nmemb > UINT_MAX || size > UINT_MAX) {
    return NULL;
  }
  return est_calloc(picorb_mrubyc_estalloc, (unsigned int)nmemb, (unsigned int)size);
}

void *
realloc(void *ptr, size_t size)
{
  if (picorb_mrubyc_estalloc == NULL) {
    return NULL;
  }
  if (size > UINT_MAX) {
    return NULL;
  }
  return est_realloc(picorb_mrubyc_estalloc, ptr, (unsigned int)size);
}

void
free(void *ptr)
{
  if (picorb_mrubyc_estalloc == NULL) {
    return;
  }
  est_free(picorb_mrubyc_estalloc, ptr);
}
