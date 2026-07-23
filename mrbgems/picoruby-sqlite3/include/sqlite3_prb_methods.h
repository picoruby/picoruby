#ifndef SQLITE3_FILE_DEFINED_H_
#define SQLITE3_FILE_DEFINED_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(PICORB_VM_MRUBY)
  #include <mruby.h>
#endif

#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PATHNAME_MAX_LEN 512

/*
 * SQLite's per-file handle. SQLite allocates it through our own allocator, so
 * it is plain C memory that the GC does not scan. The File object it holds is
 * therefore kept alive explicitly with mrb_gc_register() between
 * prb_file_new() and prb_file_close().
 */
typedef struct PRBFile
{
  sqlite3_file base;
#if defined(PICORB_VM_MRUBY)
  mrb_state *mrb;
  mrb_value file;
#endif
  int sector_size;
} PRBFile;

/*
 * Bridge from SQLite's OS layer to the VFS driver object that picoruby-vfs
 * mounted (a Littlefs instance, a FAT instance, ...). The driver is reached by
 * calling its Ruby methods, so any filesystem implementing the picoruby-vfs
 * driver protocol works and no filesystem specific header is needed here.
 *
 * Implemented per VM under src/<vm>/sqlite3_prb_methods.c. None of these lets
 * a Ruby exception escape: SQLite's C frames sit between the VM and us, and
 * unwinding through them would strand SQLite's state, so an exception is
 * caught and reported as a failure return value instead.
 */

#if defined(PICORB_VM_MRUBY)
void prb_vfs_set_driver(mrb_state *mrb, mrb_value driver);
void prb_vfs_forget_driver(mrb_state *mrb);
/*
 * Stops the bridge from calling into the VM. A GC free function must not run
 * Ruby code, yet sqlite3_close_v2() and sqlite3_finalize() can reach the VFS,
 * so those calls are made with the bridge suspended: SQLite then sees plain
 * I/O errors and tears the handle down without the VM being touched.
 */
void prb_vfs_suspend_calls(bool suspend);
#endif

bool prb_vfs_driver_set_p(void);

int64_t prb_time_gettime_us(void);

int prb_file_new(PRBFile *prbfile, const char *zName, int flags);
int prb_file_close(PRBFile *prbfile);
int prb_file_read(PRBFile *prbfile, void *zBuf, size_t nBuf);
int prb_file_write(PRBFile *prbfile, const void *zBuf, size_t nBuf);
int prb_file_fsync(PRBFile *prbfile);
int prb_file_seek(PRBFile *prbfile, sqlite3_int64 offset);
int prb_file_tell(PRBFile *prbfile);
sqlite3_int64 prb_file_size(PRBFile *prbfile);
int prb_file_unlink(const char *zName);
bool prb_file_exist_q(const char *zName);

/* sqlite3_mem_methods backed by the PicoRuby heap */
void *prb_mem_alloc(int nByte);
void prb_mem_free(void *pPrior);
void *prb_mem_realloc(void *pPrior, int nByte);
int prb_mem_size(void *p);
int prb_mem_roundup(int n);
int prb_mem_init(void *ignore);
void prb_mem_shutdown(void *ignore);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_FILE_DEFINED_H_ */
