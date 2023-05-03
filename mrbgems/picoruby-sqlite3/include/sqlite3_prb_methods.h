#ifndef SQLITE3_FILE_DEFINED_H_
#define SQLITE3_FILE_DEFINED_H_

#include <mrubyc.h>
#include "sqlite3_vfs_methods.h"
#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"
#ifdef MRBC_ALLOC_LIBC
#include <malloc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PATHNAME_MAX_LEN 512

typedef struct PRBFile
{
  sqlite3_file base;
  mrbc_vm *vm;
  mrbc_value *file;
  char pathname[PATHNAME_MAX_LEN];
  int sector_size;
} PRBFile;

mrbc_int_t prb_time_gettime_us(void);

int prb_file_new(PRBFile *prbfile, const char *zName, int flags);
int prb_file_close(PRBFile *prbfile);
int prb_file_read(PRBFile *prbfile, void *zBuf, size_t nBuf);
int prb_file_write(PRBFile *prbfile, const void *zBuf, size_t nBuf);
int prb_file_fsync(PRBFile *prbfile);
int prb_file_seek(PRBFile *prbfile, int offset);
int prb_file_tell(PRBFile *prbfile);
int prb_file_unlink(sqlite3_vfs *pVfs, const char *zName);
int prb_file_exist_q(sqlite3_vfs *pVfs, const char *zName);
int prb_file_stat(sqlite3_vfs *pVfs, const char *zName, int flags);

void vfs_funcall(
  void (*func)(mrbc_vm *, mrbc_value *, int),
  mrbc_value *v,
  int argc
);

int prb_mem_msize(void *p);
int prb_mem_roundup(int n);
int prb_mem_init(void *ignore);
void prb_mem_shutdown(void *ignore);
void *prb_raw_alloc(int nByte);
void prb_raw_free(void *pPrior);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_FILE_DEFINED_H_ */

