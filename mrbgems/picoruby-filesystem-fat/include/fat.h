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

