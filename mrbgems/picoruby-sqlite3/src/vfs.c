#include <mrubyc.h>

#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"
#include "../include/sqlite3_vfs.h"

#define MAX_PATHNAME 512

typedef mrbc_vm    prb_vm;
typedef mrbc_value prb_value;
typedef struct PRBFile
{
  sqlite3_file base;
  prb_vm *vm;
  prb_value *file;
  char pathname[MAX_PATHNAME];
} PRBFile;


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

int prb_file_open(PRBFile *prbfile, const char *zName, int flags);
int prb_file_close(PRBFile *prbfile);
int prb_file_read(PRBFile *prbfile, void *zBuf, size_t nBuf);
int prb_file_write(PRBFile *prbfile, const void *zBuf, size_t nBuf);
int prb_file_fsync(PRBFile *prbfile);
int prb_file_seek(PRBFile *prbfile, int offset, int whence);
int prb_file_tell(PRBFile *prbfile);
int prb_file_unlink(const char *zName);
int prb_file_exist_q(const char *zName);
int prb_file_stat(const char *zName, int flags);

int
sqlite3_os_init(void)
{
  return sqlite3_vfs_register(&prbvfs, 1);
}

int
sqlite3_os_end(void)
{
  return SQLITE_OK;
}

int
prb_file_open(PRBFile *prbfile, const char *zName, int flags)
{
  prb_vm *vm = (prb_vm *)prbfile->vm;
  prb_value v[3];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  v[2] = mrbc_integer_value(flags);
  vfs_methods.file_new(vm, &v[0], 2);
  if (v->tt == MRBC_TT_NIL) {
    return -1;
  }
  memcpy(prbfile->file, &v[0], sizeof(prb_value));
  return 0;
}

static int
prbvfsOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  memset(prbfile, 0, sizeof(PRBFile));
  pFile->pMethods = &prbvfs_io_methods;
  if (prb_file_open(prbfile, zName, flags) != 0) {
    return SQLITE_CANTOPEN;
  }
  if (pOutFlags) {
    *pOutFlags = flags;
  }
  prbfile->vm = (prb_vm *)pVfs->pAppData;
  return SQLITE_OK;
}

static int
prbvfsDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir)
{
  return prb_file_unlink(zName);
}

static int
prbvfsAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut)
{
  assert(flags == SQLITE_ACCESS_EXISTS || flags == SQLITE_ACCESS_READWRITE);
  int rc;
  if (flags == SQLITE_ACCESS_EXISTS) {
    rc = prb_file_exist_q(zName);
  } else {
    rc = prb_file_stat(zName, flags);
  }
  if (rc == 0) {
    *pResOut = 1;
  } else {
    *pResOut = 0;
  }
  return SQLITE_OK;
}

static int
prbvfsFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut)
{
  strncpy(zOut, zName, nOut);
  zOut[nOut-1] = '\0';
  return SQLITE_OK;
}

static int
prbvfsRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut)
{
  return SQLITE_OK;
}

static int
prbvfsClose(sqlite3_file *pFile)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_close(prbfile);
}

static int
prbvfsRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_read(prbfile, zBuf, iAmt);
}


static int
prbvfsWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_write(prbfile, zBuf, iAmt);
}

static int
prbvfsTruncate(sqlite3_file *pFile, sqlite3_int64 size)
{
  return SQLITE_OK;
}

static int
prbvfsSync(sqlite3_file *pFile, int flags)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_fsync(prbfile);
}

static int
prbvfsFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  prb_vm *vm = (prb_vm *)prbfile->vm;
  prb_value v[2];
  v[0] = mrbc_nil_value();
  v[1] = prbfile->file[0];
  vfs_methods.file_size(vm, &v[0], 1);
  *pSize = v[0].i;
  return SQLITE_OK;
}

static int
prbvfsLock(sqlite3_file *pFile, int eLock)
{
  return SQLITE_OK;
}

static int
prbvfsUnlock(sqlite3_file *pFile, int eLock)
{
  return SQLITE_OK;
}

static int
prbvfsCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
  *pResOut = 0;
  return SQLITE_OK;
}

static int
prbvfsFileControl(sqlite3_file *pFile, int op, void *pArg)
{
  return SQLITE_OK;
}

static int
prbvfsSectorSize(sqlite3_file *pFile)
{
  return 512;
}

static int
prbvfsDeviceCharacteristics(sqlite3_file *pFile)
{
  return 0;
}


