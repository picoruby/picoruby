#include <mrubyc.h>

#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"
#include "../include/sqlite3_vfs.h"

#define MAX_PATHNAME 512

#include <stdio.h>

//#define D() printf("debug: %s\n", __func__)
#define D() (void)0

typedef struct PRBFile
{
  sqlite3_file base;
  mrbc_vm *vm;
  mrbc_value *file;
  char pathname[MAX_PATHNAME];
} PRBFile;

void
sqlite3Analyze(void *pParse, void *pName1, void *pName2)
{
  D();
}

static int prbvfsOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags);
static int prbvfsDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir);
static int prbvfsAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut);
static int prbvfsFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut);
static int prbvfsRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut);
static int prbvfsClose(sqlite3_file *pFile);

static int prbvfsRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst);
static int prbvfsWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst);
static int prbvfsTruncate(sqlite3_file *pFile, sqlite3_int64 size);
static int prbvfsSync(sqlite3_file *pFile, int flags);
static int prbvfsFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize);
static int prbvfsLock(sqlite3_file *pFile, int eLock);
static int prbvfsUnlock(sqlite3_file *pFile, int eLock);
static int prbvfsCheckReservedLock(sqlite3_file *pFile, int *pResOut);
static int prbvfsFileControl(sqlite3_file *pFile, int op, void *pArg);
static int prbvfsSectorSize(sqlite3_file *pFile);
static int prbvfsDeviceCharacteristics(sqlite3_file *pFile);


static sqlite3_vfs prbvfs = {
  1,                            /* iVersion */
  sizeof(PRBFile),              /* szOsFile */
  MAX_PATHNAME,                 /* mxPathname */
  0,                            /* pNext */
  VFS_NAME,                     /* zName */
  0,                            /* pAppData */
  prbvfsOpen,                   /* xOpen */
  prbvfsDelete,                 /* xDelete */
  prbvfsAccess,                 /* xAccess */
  prbvfsFullPathname,           /* xFullPathname */
  0,                            /* xDlOpen */
  0,                            /* xDlError */
  0,                            /* xDlSym */
  0,                            /* xDlClose */
  prbvfsRandomness,             /* xRandomness */
  0,                            /* xSleep */
  0,                            /* xCurrentTime */
  0,                            /* xGetLastError */
  0,                            /* xCurrentTimeInt64 */
  0,                            /* xSetSystemCall */
  0,                            /* xGetSystemCall */
  0,                            /* xNextSystemCall */
};

void
set_vm_for_vfs(mrbc_vm *vm)
{
  if (!prbvfs.pAppData) {
    prbvfs.pAppData = vm;
  }
}

mrbc_vm *
get_vm_for_vfs(void)
{
  return prbvfs.pAppData;
}

static sqlite3_io_methods prbvfs_io_methods = {
  3,                            /* iVersion */
  prbvfsClose,                  /* xClose */
  prbvfsRead,                   /* xRead */
  prbvfsWrite,                  /* xWrite */
  prbvfsTruncate,               /* xTruncate */
  prbvfsSync,                   /* xSync */
  prbvfsFileSize,               /* xFileSize */
  prbvfsLock,                   /* xLock */
  prbvfsUnlock,                 /* xUnlock */
  prbvfsCheckReservedLock,      /* xCheckReservedLock */
  prbvfsFileControl,            /* xFileControl */
  prbvfsSectorSize,             /* xSectorSize */
  prbvfsDeviceCharacteristics,  /* xDeviceCharacteristics */
  0,                            /* xShmMap */
  0,                            /* xShmLock */
  0,                            /* xShmBarrier */
  0,                            /* xShmUnmap */
  0,                            /* xFetch */
  0                             /* xUnfetch */
};

int prb_file_new(PRBFile *prbfile, const char *zName, int flags);
int prb_file_close(PRBFile *prbfile);
int prb_file_read(PRBFile *prbfile, void *zBuf, size_t nBuf);
int prb_file_write(PRBFile *prbfile, const void *zBuf, size_t nBuf);
int prb_file_fsync(PRBFile *prbfile);
int prb_file_seek(PRBFile *prbfile, int offset, int whence);
int prb_file_tell(PRBFile *prbfile);
int prb_file_unlink(sqlite3_vfs *pVfs, const char *zName);
int prb_file_exist_q(sqlite3_vfs *pVfs, const char *zName);
int prb_file_stat(sqlite3_vfs *pVfs, const char *zName, int flags);

#ifdef MRBC_ALLOC_LIBC
#include <malloc.h>
#endif
int
prb_mem_msize(void *p)
{
  D();
#ifdef MRBC_ALLOC_LIBC
  return malloc_usable_size(p);
#else
  return mrbc_alloc_usable_size(p);
#endif
}

int
prb_mem_roundup(int n)
{
  D();
  return n;
}

int
prb_mem_init(void *ignore)
{
  D();
  return SQLITE_OK;
}

void
prb_mem_shutdown(void *ignore)
{
  D();
}


void *
prb_raw_alloc(int nByte)
{
  void *ptr = mrbc_raw_alloc(nByte);
  //console_printf("prb_raw_alloc(%d) = %p\n", nByte, ptr);
  return ptr;
}

void
prb_raw_free(void *pPrior)
{
  //console_printf("prb_raw_free(%p)\n", pPrior);
  mrbc_raw_free(pPrior);
}

int
sqlite3_os_init(void)
{
  D();
  static const sqlite3_mem_methods defaultMethods = {
    prb_raw_alloc,
    prb_raw_free,
    (void *(*)(void*, int))mrbc_raw_realloc,
    prb_mem_msize,
    prb_mem_roundup,
    prb_mem_init,
    prb_mem_shutdown,
    0
  };
  sqlite3_config(SQLITE_CONFIG_MALLOC, &defaultMethods);
  sqlite3_initialize();
  return sqlite3_vfs_register(&prbvfs, 1);
}

int
sqlite3_os_end(void)
{
  D();
  return SQLITE_OK;
}

static void
vfs_funcall(
  void (*func)(mrbc_vm *, mrbc_value *, int),
  mrbc_value *v,
  int argc
)
{
  mrbc_incref(&v[0]);
  func(prbvfs.pAppData, &v[0], argc);
}

int
prb_file_new(PRBFile *prbfile, const char *zName, int flags)
{
  D();
  mrbc_vm *vm = (mrbc_vm *)prbfile->vm;
  mrbc_value v[3];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  //v[2] = mrbc_integer_value(flags);
  /*
   * TODO: handle flags like SQLITE_OPEN_READONLY, SQLITE_OPEN_READWRITE, SQLITE_OPEN_CREATE
   */
  v[2] = mrbc_string_new_cstr(vm, "w+");
  vfs_funcall(vfs_methods.file_new, &v[0], 2);
  if (v->tt == MRBC_TT_NIL) {
    return -1;
  }
  prbfile->file = mrbc_alloc(vm, sizeof(mrbc_value));
  memcpy(prbfile->file, &v[0], sizeof(mrbc_value));
  return 0;
}

int prb_file_close(PRBFile *prbfile)
{
  D();
  mrbc_value v[1];
  v[0] = *prbfile->file;
  vfs_funcall(vfs_methods.file_close, &v[0], 0);
  return 0;
}

int prb_file_read(PRBFile *prbfile, void *zBuf, size_t nBuf)
{
  D();
  mrbc_value v[2];
  v[0] = *prbfile->file;
  v[1] = mrbc_integer_value(nBuf);
  vfs_funcall(vfs_methods.file_read, &v[0], 1);
  if (v->tt == MRBC_TT_NIL) {
    return 0; /* EOF */
  } else if (v->tt != MRBC_TT_STRING) {
    return -1;
  }
  memcpy(zBuf, v[0].string->data, v[0].string->size);
  return v[0].string->size;
}

int prb_file_write(PRBFile *prbfile, const void *zBuf, size_t nBuf)
{
  D();
  mrbc_vm *vm = (mrbc_vm *)prbfile->vm;
  mrbc_value v[3];
  v[0] = *prbfile->file;
  v[1] = mrbc_string_new(vm, zBuf, nBuf);
  v[2] = mrbc_integer_value(nBuf);
  vfs_funcall(vfs_methods.file_write, &v[0], 2);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return v->i;
}

int prb_file_fsync(PRBFile *prbfile)
{
  D();
  mrbc_value v[1];
  v[0] = *prbfile->file;
  vfs_funcall(vfs_methods.file_fsync, &v[0], 0);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return 0;
}

int prb_file_seek(PRBFile *prbfile, int offset, int whence)
{
  D();
  mrbc_value v[3];
  v[0] = *prbfile->file;
  v[1] = mrbc_integer_value(offset);
  v[2] = mrbc_integer_value(whence);
  vfs_funcall(vfs_methods.file_seek, &v[0], 2);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return 0;
}

int prb_file_tell(PRBFile *prbfile)
{
  D();
  mrbc_value v[1];
  v[0] = *prbfile->file;
  vfs_funcall(vfs_methods.file_tell, &v[0], 0);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return v->i;
}

int prb_file_unlink(sqlite3_vfs *pVfs, const char *zName)
{
  D();
  mrbc_vm *vm = prbvfs.pAppData;
  mrbc_value v[2];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  vfs_funcall(vfs_methods.file_unlink, &v[0], 1);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return 0;
}

int prb_file_exist_q(sqlite3_vfs *pVfs, const char *zName)
{
  D();
  mrbc_vm *vm = prbvfs.pAppData;
  mrbc_value v[2];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  vfs_funcall(vfs_methods.file_exist_q, &v[0], 1);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return (v->tt == MRBC_TT_TRUE) ? 0 : -1;
}

int prb_file_stat(sqlite3_vfs *pVfs, const char *zName, int stat)
{
  D();
  mrbc_vm *vm = prbvfs.pAppData;
  mrbc_value v[2];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  vfs_funcall(vfs_methods.file_stat, &v[0], 1);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return 0;
}



static int
prbvfsOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  memset(prbfile, 0, sizeof(PRBFile));
  prbfile->vm = prbvfs.pAppData;
  pFile->pMethods = &prbvfs_io_methods;
  memcpy(prbfile->pathname, zName, strlen(zName));
  if (prb_file_new(prbfile, zName, flags) != 0) {
    return SQLITE_CANTOPEN;
  }
  if (pOutFlags) {
    *pOutFlags = flags;
  }
  return SQLITE_OK;
}

static int
prbvfsDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir)
{
  D();
  return prb_file_unlink(pVfs, zName);
}

static int
prbvfsAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut)
{
  D();
  assert(flags == SQLITE_ACCESS_EXISTS
      || flags == SQLITE_ACCESS_READ
      || flags == SQLITE_ACCESS_READWRITE);
  int rc;
  if (flags == SQLITE_ACCESS_EXISTS) {
    rc = prb_file_exist_q(pVfs, zName);
  } else {
    rc = prb_file_stat(pVfs, zName, flags);
  }
  if (rc == 0) {
    *pResOut = 0;
  } else {
    *pResOut = 0; // TODO
  }
  return SQLITE_OK;
}

static int
prbvfsFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut)
{
  D();
  strncpy(zOut, zName, nOut);
  zOut[nOut+1] = '\0';
  return SQLITE_OK;
}

static int
prbvfsRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut)
{
  D();
  return SQLITE_OK;
}

static int
prbvfsClose(sqlite3_file *pFile)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_close(prbfile);
}

static int
prbvfsRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return (iAmt = prb_file_read(prbfile, zBuf, iAmt)) ? SQLITE_OK : SQLITE_IOERR_SHORT_READ;
}


static int
prbvfsWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return (iAmt == prb_file_write(prbfile, zBuf, iAmt)) ? SQLITE_OK : SQLITE_IOERR_WRITE;
}

static int
prbvfsTruncate(sqlite3_file *pFile, sqlite3_int64 size)
{
  D();
  return SQLITE_OK;
}

static int
prbvfsSync(sqlite3_file *pFile, int flags)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_fsync(prbfile);
}

static int
prbvfsFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  mrbc_value v[1];
  v[0] = *prbfile->file;
  vfs_funcall(vfs_methods.file_size, &v[0], 0);
  *pSize = v[0].i;
  return SQLITE_OK;
}

static int
prbvfsLock(sqlite3_file *pFile, int eLock)
{
  D();
  return SQLITE_OK;
}

static int
prbvfsUnlock(sqlite3_file *pFile, int eLock)
{
  D();
  return SQLITE_OK;
}

static int
prbvfsCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
  D();
  *pResOut = 0;
  return SQLITE_OK;
}

static int
prbvfsFileControl(sqlite3_file *pFile, int op, void *pArg)
{
  D();
  return SQLITE_NOTFOUND;
}

static int
prbvfsSectorSize(sqlite3_file *pFile)
{
  D();
  return 512;
}

static int
prbvfsDeviceCharacteristics(sqlite3_file *pFile)
{
  D();
  return 0;
}


