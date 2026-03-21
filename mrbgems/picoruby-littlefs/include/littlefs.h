#ifndef LITTLEFS_DEFINED_H_
#define LITTLEFS_DEFINED_H_

#if defined(PICORB_VM_MRUBY)
  #include <mruby.h>
#elif defined(PICORB_VM_MRUBYC)
  #include <mrubyc.h>
#endif

#include "../lib/littlefs/lfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Custom attribute type IDs for metadata storage */
#define LFS_ATTR_MTIME 0x01

/* File wrapper: stores handle + path + writable flag for auto-mtime on close */
typedef struct {
  lfs_file_t file;               /* must be first member */
  char path[LFS_NAME_MAX + 1];
  bool writable;
} lfs_file_data_t;

/* Global lfs instance accessors */
lfs_t *littlefs_get_lfs(void);
struct lfs_config *littlefs_get_config(void);
int littlefs_ensure_mounted(void);
uint32_t littlefs_get_unixtime(void);

/* Port-specific HAL must implement these */
void littlefs_hal_init_config(struct lfs_config *cfg);
void littlefs_hal_erase_all(void);

int LFS_prepare_exception(int err, char *buff, const char *func);

#if defined(PICORB_VM_MRUBY)

typedef struct prb_vfs_methods
{
  mrb_value (*file_new)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_close)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_read)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_getbyte)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_write)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_seek)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_tell)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_size)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_fsync)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_exist_q)(mrb_state *mrb, mrb_value self);
  mrb_value (*file_unlink)(mrb_state *mrb, mrb_value self);
} prb_vfs_methods;

mrb_value mrb__exist_p(mrb_state *mrb, mrb_value klass);
mrb_value mrb__unlink(mrb_state *mrb, mrb_value klass);
mrb_value mrb__rename(mrb_state *mrb, mrb_value klass);

void mrb_raise_iff_lfs_error(mrb_state *mrb, int err, const char *func);
void mrb_init_class_Littlefs_File(mrb_state *mrb, struct RClass *class_LFS);
void mrb_init_class_Littlefs_Dir(mrb_state *mrb, struct RClass *class_LFS);

#elif defined(PICORB_VM_MRUBYC)

typedef struct prb_vfs_methods
{
  void (*file_new)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_close)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_read)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_getbyte)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_write)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_seek)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_tell)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_size)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_fsync)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_exist_q)(mrbc_vm *vm, mrbc_value *v, int argc);
  void (*file_unlink)(mrbc_vm *vm, mrbc_value *v, int argc);
} prb_vfs_methods;

void c__exist_q(mrbc_vm *vm, mrbc_value v[], int argc);
void c__unlink(mrbc_vm *vm, mrbc_value v[], int argc);
void c__rename(mrbc_vm *vm, mrbc_value v[], int argc);

void mrbc_raise_iff_lfs_error(mrbc_vm *vm, int err, const char *func);
void mrbc_init_class_Littlefs_File(mrbc_vm *vm, mrbc_class *class_LFS);
void mrbc_init_class_Littlefs_Dir(mrbc_vm *vm, mrbc_class *class_LFS);

#endif

#ifdef __cplusplus
}
#endif

#endif /* LITTLEFS_DEFINED_H_ */
