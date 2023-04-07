#ifndef SQLITE3_VFS_DEFINED_H_
#define SQLITE3_VFS_DEFINED_H_

#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif


#define VFS_NAME "prbvfs"

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

extern prb_vfs_methods vfs_methods;

void set_vm_for_vfs(mrbc_vm *vm);
mrbc_vm *get_vm_for_vfs(void);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_VFS_DEFINED_H_ */

