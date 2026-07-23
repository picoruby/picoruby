#ifndef SQLITE3_VFS_DEFINED_H_
#define SQLITE3_VFS_DEFINED_H_

#include "sqlite3_prb_methods.h"
#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_NAME "prb_vfs"

int prbVFSOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags);
int prbVFSDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir);
int prbVFSAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut);
int prbVFSFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut);
int prbVFSRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut);
int prbVFSCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *piNow);

int prbIOClose(sqlite3_file *pFile);
int prbIORead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst);
int prbIOWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst);
int prbIOTruncate(sqlite3_file *pFile, sqlite3_int64 size);
int prbIOSync(sqlite3_file *pFile, int flags);
int prbIOFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize);
int prbIOLock(sqlite3_file *pFile, int eLock);
int prbIOUnlock(sqlite3_file *pFile, int eLock);
int prbIOCheckReservedLock(sqlite3_file *pFile, int *pResOut);
int prbIOFileControl(sqlite3_file *pFile, int op, void *pArg);
int prbIOSectorSize(sqlite3_file *pFile);
int prbIODeviceCharacteristics(sqlite3_file *pFile);

extern sqlite3_vfs prb_vfs;

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_VFS_DEFINED_H_ */
