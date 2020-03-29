#ifndef MMRBC_FMEMSTREAM_H_
#define MMRBC_FMEMSTREAM_H_

typedef struct fmemstream
{
  int pos;
  char *mem;
  int size;
} fmemstream;

fmemstream *fmemstreamopen(char *mem);

void fmemstreamclose(fmemstream *self);

char *fmemgets(char *s, int n, FILE *fp);

int fmemeof(FILE *fp);

#endif /* MMRBC_FMEMSTREAM_H_ */
