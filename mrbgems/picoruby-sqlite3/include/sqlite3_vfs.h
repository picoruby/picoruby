#ifndef SQLITE3_VFS_DEFINED_H_
#define SQLITE3_VFS_DEFINED_H_

#include <mrubyc.h>
#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"
#include "sqlite3_file.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VFS_NAME "prbvfs"

int prbvfsOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *pFile, int flags, int *pOutFlags);
int prbvfsDelete(sqlite3_vfs *pVfs, const char *zName, int syncDir);
int prbvfsAccess(sqlite3_vfs *pVfs, const char *zName, int flags, int *pResOut);
int prbvfsFullPathname(sqlite3_vfs *pVfs, const char *zName, int nOut, char *zOut);
int prbvfsRandomness(sqlite3_vfs *pVfs, int nByte, char *zOut);
int prbvfsClose(sqlite3_file *pFile);
int prbvfsRead(sqlite3_file *pFile, void *zBuf, int iAmt, sqlite3_int64 iOfst);
int prbvfsWrite(sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite3_int64 iOfst);
int prbvfsTruncate(sqlite3_file *pFile, sqlite3_int64 size);
int prbvfsSync(sqlite3_file *pFile, int flags);
int prbvfsFileSize(sqlite3_file *pFile, sqlite3_int64 *pSize);
int prbvfsLock(sqlite3_file *pFile, int eLock);
int prbvfsUnlock(sqlite3_file *pFile, int eLock);
int prbvfsCheckReservedLock(sqlite3_file *pFile, int *pResOut);
int prbvfsFileControl(sqlite3_file *pFile, int op, void *pArg);
int prbvfsSectorSize(sqlite3_file *pFile);
int prbvfsDeviceCharacteristics(sqlite3_file *pFile);

typedef struct prb_vfs_methods
{
  void (*file_new)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_close)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_read)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_write)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_seek)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_tell)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_size)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_fsync)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_exist_q)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_unlink)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_stat)(mrbc_vm *vm, mrbc_value *v, int argc);
} prb_vfs_methods;

extern sqlite3_vfs prbvfs;
extern prb_vfs_methods vfs_methods;

void set_vm_for_vfs(mrbc_vm *vm);
mrbc_vm *get_vm_for_vfs(void);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_VFS_DEFINED_H_ */

