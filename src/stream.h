#ifndef MMRBC_FMEMSTREAM_H_
#define MMRBC_FMEMSTREAM_H_

typedef enum stream_type
{
  STREAM_TYPE_FILE,
  STREAM_TYPE_MEMORY
} StreamType;

typedef struct stream_interface
{
  StreamType type;
  void *stream;
  char *(*fgetsProc)(char *s, int n, FILE *stream);
  int (*feofProc)(FILE *stream);
} StreamInterface;

typedef struct fmemstream
{
  int pos;
  char *mem;
  int size;
} fmemstream;

StreamInterface *StreamInterface_new(char *c, StreamType st);

void StreamInterface_free(StreamInterface *si);

#endif /* MMRBC_FMEMSTREAM_H_ */
