
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
} prb_vfs_methods;

prb_vfs_methods vfs_methods;

