#include "../include/sqlite3_vfs_methods.h"

#include <string.h>

/*
 * SQLite's OS layer. Everything here is VM independent: it only talks to
 * SQLite and to the prb_* bridge declared in sqlite3_prb_methods.h.
 */

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

/*
 * SQLITE_OMIT_ANALYZE removes the implementation but not every reference to
 * it, so a stub is still needed at link time.
 */
void
sqlite3Analyze(void *pParse, void *pName1, void *pName2)
{
}

static sqlite3_io_methods prb_io_methods = {
  3,                            /* iVersion */
  prbIOClose,                   /* xClose */
  prbIORead,                    /* xRead */
  prbIOWrite,                   /* xWrite */
  prbIOTruncate,                /* xTruncate */
  prbIOSync,                    /* xSync */
  prbIOFileSize,                /* xFileSize */
  prbIOLock,                    /* xLock */
  prbIOUnlock,                  /* xUnlock */
  prbIOCheckReservedLock,       /* xCheckReservedLock */
  prbIOFileControl,             /* xFileControl */
  prbIOSectorSize,              /* xSectorSize */
  prbIODeviceCharacteristics,   /* xDeviceCharacteristics */
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
  static const sqlite3_mem_methods prb_mem_methods = {
    prb_mem_alloc,
    prb_mem_free,
    prb_mem_realloc,
    prb_mem_size,
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
  return SQLITE_OK;
}

int
prbVFSOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  memset(prbfile, 0, sizeof(PRBFile));
  pFile->pMethods = &prb_io_methods;
  if (prb_file_new(prbfile, zName, flags) != 0) {
    /* SQLite only calls xClose when pMethods is set, so clear it again */
    pFile->pMethods = NULL;
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
  if (prb_file_unlink(zName) != 0) {
    return SQLITE_IOERR_DELETE;
  }
  return SQLITE_OK;
}

int
prbVFSAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut)
{
  /*
   * There is a single user and no permission bits on the filesystems
   * PicoRuby mounts, so an existing file is always readable and writable.
   */
  *pResOut = prb_file_exist_q(zName) ? 1 : 0;
  return SQLITE_OK;
}

int
prbVFSFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut)
{
  /* Paths handed to SQLite are already absolute within the mounted volume */
  sqlite3_snprintf(nOut, zOut, "%s", zName);
  return SQLITE_OK;
}

int
prbVFSRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut)
{
  /*
   * Only used to pick temporary file names and journal nonces. There is no
   * entropy source on the target boards, so mix the clock with a counter that
   * keeps successive calls distinct.
   */
  static uint32_t seq = 0;
  uint64_t state = (uint64_t)prb_time_gettime_us() ^ ((uint64_t)(++seq) << 32);
  int i = 0;
  while (i < nByte) {
    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
    zOut[i] = (char)(state >> 33);
    i++;
  }
  return nByte;
}

int
prbVFSCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow)
{
  static const sqlite3_int64 unixEpoch = 24405875 * (sqlite3_int64)8640000;
  *piNow = unixEpoch + prb_time_gettime_us() / 1000;
  return SQLITE_OK;
}

int
prbIOClose(sqlite3_file *pFile)
{
  return prb_file_close((PRBFile *)pFile);
}

int
prbIORead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  if (prb_file_seek(prbfile, iOfst) < 0) {
    return SQLITE_IOERR_READ;
  }
  int nRead = prb_file_read(prbfile, zBuf, (size_t)iAmt);
  if (nRead < 0) {
    return SQLITE_IOERR_READ;
  }
  if (nRead < iAmt) {
    /* SQLite requires the unread tail to be zeroed on a short read */
    memset((char *)zBuf + nRead, 0, (size_t)(iAmt - nRead));
    return SQLITE_IOERR_SHORT_READ;
  }
  return SQLITE_OK;
}

int
prbIOWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst)
{
  PRBFile *prbfile = (PRBFile *)pFile;
  if (prb_file_seek(prbfile, iOfst) < 0) {
    return SQLITE_IOERR_WRITE;
  }
  return (iAmt == prb_file_write(prbfile, zBuf, (size_t)iAmt)) ? SQLITE_OK : SQLITE_IOERR_WRITE;
}

int
prbIOTruncate(sqlite3_file *pFile, sqlite3_int64 size)
{
  /* The VFS driver protocol has no truncate; see README for the consequence */
  return SQLITE_OK;
}

int
prbIOSync(sqlite3_file *pFile, int flags)
{
  return (prb_file_fsync((PRBFile *)pFile) == 0) ? SQLITE_OK : SQLITE_IOERR_FSYNC;
}

int
prbIOFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize)
{
  sqlite3_int64 size = prb_file_size((PRBFile *)pFile);
  if (size < 0) {
    return SQLITE_IOERR_FSTAT;
  }
  *pSize = size;
  return SQLITE_OK;
}

int
prbIOLock(sqlite3_file *pFile, int eLock)
{
  return SQLITE_OK;
}

int
prbIOUnlock(sqlite3_file *pFile, int eLock)
{
  return SQLITE_OK;
}

int
prbIOCheckReservedLock(sqlite3_file *pFile, int *pResOut)
{
  *pResOut = 0;
  return SQLITE_OK;
}

int
prbIOFileControl(sqlite3_file *pFile, int op, void *pArg)
{
  return SQLITE_NOTFOUND;
}

int
prbIOSectorSize(sqlite3_file *pFile)
{
  return ((PRBFile *)pFile)->sector_size;
}

int
prbIODeviceCharacteristics(sqlite3_file *pFile)
{
  return 0;
}
