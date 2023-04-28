#include "../include/sqlite3_vfs.h"

#include <stdio.h>

//#define D() printf("debug: %s\n", __func__)
#define D() (void)0

sqlite3_vfs prbvfs = {
  3,                            /* iVersion */
  sizeof(PRBFile),              /* szOsFile */
  PATHNAME_MAX_LEN,                 /* mxPathname */
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
  prbvfsCurrentTimeInt64,       /* xCurrentTimeInt64 */
  0,                            /* xSetSystemCall */
  0,                            /* xGetSystemCall */
  0,                            /* xNextSystemCall */
};

void
sqlite3Analyze(void *pParse, void *pName1, void *pName2)
{
  D();
}



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
  return ptr;
}

void
prb_raw_free(void *pPrior)
{
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


int
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

int
prbvfsDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir)
{
  D();
  return prb_file_unlink(pVfs, zName);
}

int
prbvfsAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut)
{
  D();
  assert(flags == SQLITE_ACCESS_EXISTS
      || flags == SQLITE_ACCESS_READ
      || flags == SQLITE_ACCESS_READWRITE);
  // TODO
  if (flags == SQLITE_ACCESS_EXISTS) {
    if (prb_file_exist_q(pVfs, zName)) {
      *pResOut = 1;
    } else {
      *pResOut = 0;
    }
  } else {
    if (prb_file_stat(pVfs, zName, flags) == 0) {
      *pResOut = 0;
    } else {
      *pResOut = 1;
    }
  }
  return SQLITE_OK;
}

int
prbvfsFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut)
{
  D();
  strncpy(zOut, zName, nOut);
  zOut[nOut+1] = '\0';
  return SQLITE_OK;
}

int
prbvfsRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut)
{
  D();
  return SQLITE_OK;
}

int
prbvfsCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow)
{
  D();
  static const sqlite3_int64 unixEpoch = 24405875 * (sqlite3_int64)8640000;
  *piNow = unixEpoch + prb_time_gettime_us();
  return SQLITE_OK;
}

int
prbvfsClose(sqlite3_file *pFile)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_close(prbfile);
}

int
prbvfsRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  if (prb_file_seek(prbfile, iOfst) < 0) {
    return SQLITE_ERROR;
  }
  return (iAmt = prb_file_read(prbfile, zBuf, iAmt)) ? SQLITE_OK : SQLITE_IOERR_SHORT_READ;
}


int
prbvfsWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  if (prb_file_seek(prbfile, iOfst) < 0) {
    return SQLITE_ERROR;
  }
  return (iAmt == prb_file_write(prbfile, zBuf, iAmt)) ? SQLITE_OK : SQLITE_IOERR_WRITE;
}

int
prbvfsTruncate(sqlite3_file *pFile, sqlite3_int64 size)
{
  D();
  return SQLITE_OK;
}

int
prbvfsSync(sqlite3_file *pFile, int flags)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_fsync(prbfile);
}

int
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

int
prbvfsLock(sqlite3_file *pFile, int eLock)
{
  D();
  return SQLITE_OK;
}

int
prbvfsUnlock(sqlite3_file *pFile, int eLock)
{
  D();
  return SQLITE_OK;
}

int
prbvfsCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
  D();
  *pResOut = 0;
  return SQLITE_OK;
}

int
prbvfsFileControl(sqlite3_file *pFile, int op, void *pArg)
{
  D();
  return SQLITE_NOTFOUND;
}

int
prbvfsSectorSize(sqlite3_file *pFile)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return prbfile->sector_size;
}

int
prbvfsDeviceCharacteristics(sqlite3_file *pFile)
{
  D();
  return 0;
}

