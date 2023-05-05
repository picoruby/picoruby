#include "../include/sqlite3_vfs_methods.h"

#include <stdio.h>

//#define D() printf("debug: %s\n", __func__)
#define D() (void)0

sqlite3_vfs prb_vfs = {
  3,                            /* iVersion */
  sizeof(PRBFile),              /* szOsFile */
  PATHNAME_MAX_LEN,             /* mxPathname */
  0,                            /* pNext */
  VFS_NAME,                     /* zName */
  0,                            /* pAppData */
  prbVFSOpen,                   /* xOpen */
  prbVFSDelete,                 /* xDelete */
  prbVFSAccess,                 /* xAccess */
  prbVFSFullPathname,           /* xFullPathname */
  0,                            /* xDlOpen */
  0,                            /* xDlError */
  0,                            /* xDlSym */
  0,                            /* xDlClose */
  prbVFSRandomness,             /* xRandomness */
  0,                            /* xSleep */
  0,                            /* xCurrentTime */
  0,                            /* xGetLastError */
  prbVFSCurrentTimeInt64,       /* xCurrentTimeInt64 */
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
  if (!prb_vfs.pAppData) {
    prb_vfs.pAppData = vm;
  }
}

mrbc_vm *
get_vm_for_vfs(void)
{
  return prb_vfs.pAppData;
}

static sqlite3_io_methods prb_io_methods = {
  3,                            /* iVersion */
  prbIOClose,                  /* xClose */
  prbIORead,                   /* xRead */
  prbIOWrite,                  /* xWrite */
  prbIOTruncate,               /* xTruncate */
  prbIOSync,                   /* xSync */
  prbIOFileSize,               /* xFileSize */
  prbIOLock,                   /* xLock */
  prbIOUnlock,                 /* xUnlock */
  prbIOCheckReservedLock,      /* xCheckReservedLock */
  prbIOFileControl,            /* xFileControl */
  prbIOSectorSize,             /* xSectorSize */
  prbIODeviceCharacteristics,  /* xDeviceCharacteristics */
  0,                            /* xShmMap */
  0,                            /* xShmLock */
  0,                            /* xShmBarrier */
  0,                            /* xShmUnmap */
  0,                            /* xFetch */
  0                             /* xUnfetch */
};

int
sqlite3_os_init(void)
{
  D();
  static const sqlite3_mem_methods prb_mem_methods = {
    prb_raw_alloc,
    prb_raw_free,
    (void *(*)(void*, int))mrbc_raw_realloc,
    prb_mem_msize,
    prb_mem_roundup,
    prb_mem_init,
    prb_mem_shutdown,
    0
  };
  sqlite3_config(SQLITE_CONFIG_MALLOC, &prb_mem_methods);
  sqlite3_initialize();
  return sqlite3_vfs_register(&prb_vfs, 1);
}

int
sqlite3_os_end(void)
{
  D();
  return SQLITE_OK;
}

int
prbVFSOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  memset(prbfile, 0, sizeof(PRBFile));
  prbfile->vm = prb_vfs.pAppData;
  pFile->pMethods = &prb_io_methods;
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
prbVFSDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir)
{
  D();
  return prb_file_unlink(pVfs, zName);
}

int
prbVFSAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut)
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
prbVFSFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut)
{
  D();
  strncpy(zOut, zName, nOut);
  zOut[nOut+1] = '\0';
  return SQLITE_OK;
}

int
prbVFSRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut)
{
  D();
  return SQLITE_OK;
}

int
prbVFSCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow)
{
  D();
  static const sqlite3_int64 unixEpoch = 24405875 * (sqlite3_int64)8640000;
  *piNow = unixEpoch + prb_time_gettime_us();
  return SQLITE_OK;
}

int
prbIOClose(sqlite3_file *pFile)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_close(prbfile);
}

int
prbIORead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  if (prb_file_seek(prbfile, iOfst) < 0) {
    return SQLITE_ERROR;
  }
  return (iAmt == prb_file_read(prbfile, zBuf, iAmt)) ? SQLITE_OK : SQLITE_IOERR_SHORT_READ;
}


int
prbIOWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  if (prb_file_seek(prbfile, iOfst) < 0) {
    return SQLITE_ERROR;
  }
  return (iAmt == prb_file_write(prbfile, zBuf, iAmt)) ? SQLITE_OK : SQLITE_IOERR_WRITE;
}

int
prbIOTruncate(sqlite3_file *pFile, sqlite3_int64 size)
{
  D();
  return SQLITE_OK;
}

int
prbIOSync(sqlite3_file *pFile, int flags)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return prb_file_fsync(prbfile);
}

int
prbIOFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  mrbc_value v[1];
  v[0] = *prbfile->file;
  prb_funcall(vfs_methods.file_size, &v[0], 0);
  *pSize = v[0].i;
  return SQLITE_OK;
}

int
prbIOLock(sqlite3_file *pFile, int eLock)
{
  D();
  return SQLITE_OK;
}

int
prbIOUnlock(sqlite3_file *pFile, int eLock)
{
  D();
  return SQLITE_OK;
}

int
prbIOCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
  D();
  *pResOut = 0;
  return SQLITE_OK;
}

int
prbIOFileControl(sqlite3_file *pFile, int op, void *pArg)
{
  D();
  return SQLITE_NOTFOUND;
}

int
prbIOSectorSize(sqlite3_file *pFile)
{
  D();
  PRBFile *prbfile = (PRBFile *)pFile;
  return prbfile->sector_size;
}

int
prbIODeviceCharacteristics(sqlite3_file *pFile)
{
  D();
  return 0;
}

