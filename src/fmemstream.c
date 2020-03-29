#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "fmemstream.h"

fmemstream *fmemstreamopen(char *mem)
{
  fmemstream *self = mmrbc_alloc(sizeof(fmemstream));
  self->pos = 0;
  self->mem = mem;
  self->size = strlen(mem);
  return self;
}

void fmemstreamclose(fmemstream *self)
{
  mmrbc_free(self);
}

/*
 * the same interface of feof()
 */
int fmemeof(FILE *fp)
{
  fmemstream *self = (fmemstream *)fp;
  if (self->pos < self->size) {
    return 1;
  } else if (self->pos == self->size) {
    return 0;
  } else {
    FATAL("fmemeof error");
  }
  return -1;
}

/*
 * the same interface of fgets()
 */
char *fmemgets(char *s, int max, FILE *fp)
{
  if (fmemeof(fp) == 0) return NULL;
  fmemstream *self = (fmemstream *)fp;
  char *end = strchr(&self->mem[self->pos], (int)'\n');
  int len;
  if (end == NULL) {
    if ( (self->size - self->pos) < max ) {
      len = self->size - self->pos;
    } else {
      len = max - 1;
    }
  } else {
    len = 1 + (intptr_t)end - (intptr_t)&self->mem[self->pos];
  }
  if (len < max) {
    memcpy(s, &self->mem[self->pos], len);
    s[len] = '\0';
    self->pos += len;
  } else {
    memcpy(s, &self->mem[self->pos], max - 1);
    s[max] = '\0';
    self->pos += max - 1;
  }
  return s;
}

