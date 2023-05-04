#ifndef FAT_DEFINED_H_
#define FAT_DEFINED_H_

#include <mrubyc.h>

#include "../lib/ff14b/source/ff.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

void c__exist_q(mrbc_vm *vm, mrbc_value v[], int argc);
void c__unlink(mrbc_vm *vm, mrbc_value v[], int argc);
void c__rename(mrbc_vm *vm, mrbc_value v[], int argc);

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

void mrbc_raise_iff_f_error(mrbc_vm *vm, FRESULT res, const char *func);
void mrbc_init_class_FAT_File(void);
void mrbc_init_class_FAT_Dir(void);

#ifdef USE_FAT_SD_DISK
void c_FAT_init_spi();
#endif

#ifdef __cplusplus
}
#endif

#endif /* FAT_DEFINED_H_ */

