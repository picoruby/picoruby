#include <stdint.h>
#include <stdio.h>

#include "c_fat.h"
#include "c_fat_dir.h"
#include "c_fat_file.h"

#include "../lib/ff14b/source/ff.h"

static void
c__mount(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value fatfs = mrbc_instance_new(vm, v->cls, sizeof(FATFS));
  FATFS *fs = (FATFS *)fatfs.instance->data;
  FRESULT res;
  res = f_mount(fs, (const TCHAR *)GET_STRING_ARG(1), 1);
  mrbc_raise_iff_f_error(vm, res, "f_mount");
  SET_INT_RETURN(0);
}

static void
c__unmount(struct VM *vm, mrbc_value v[], int argc)
{
  FRESULT res;
  res = f_mount(0, (const TCHAR *)GET_STRING_ARG(1), 0);
  mrbc_raise_iff_f_error(vm, res, "f_unmount");
  SET_INT_RETURN(0);
}

static void
c__chdir(struct VM *vm, mrbc_value v[], int argc)
{
  FRESULT res;
  res = f_chdir((const TCHAR *)GET_STRING_ARG(1));
  mrbc_raise_iff_f_error(vm, res, "f_chdir");
  SET_INT_RETURN(0);
}


static void
c__mkdir(struct VM *vm, mrbc_value v[], int argc)
{
  FRESULT res = f_mkdir((const TCHAR *)GET_STRING_ARG(1));
  mrbc_raise_iff_f_error(vm, res, "f_mkdir");
  SET_INT_RETURN(0);
}

static void
c__unlink(mrbc_vm *vm, mrbc_value v[], int argc)
{
  TCHAR *path = (TCHAR *)GET_STRING_ARG(1);
  FRESULT res = f_unlink(path);
  mrbc_raise_iff_f_error(vm, res, "f_unlink");
  SET_INT_RETURN(0);
}

static void
c__exist_eq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  TCHAR *path = (TCHAR *)GET_STRING_ARG(1);
  FILINFO fno;
  FRESULT res = f_stat(path, &fno);
  if (res == FR_OK) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c__directory_eq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  TCHAR *path = (TCHAR *)GET_STRING_ARG(1);
  FILINFO fno;
  FRESULT res = f_stat(path, &fno);
  if (res == FR_OK && (fno.fattrib & AM_DIR)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

#define PREPARE_EXCEPTION(message) (sprintf(buff, "%s @ %s", message, func))

void
mrbc_raise_iff_f_error(mrbc_vm *vm, FRESULT res, const char *func)
{
  char buff[100];
  switch (res) {
    case FR_OK:
      return;
    case FR_DISK_ERR:
      PREPARE_EXCEPTION("Unrecoverable hard error");
      break;
    case FR_INT_ERR:
      PREPARE_EXCEPTION("Insanity is detected");
      break;
    case FR_NOT_READY:
      PREPARE_EXCEPTION("Storage device not ready");
      break;
    case FR_NO_FILE:
    case FR_NO_PATH:
      PREPARE_EXCEPTION("No such file or directory");
      break;
    case FR_INVALID_NAME:
      PREPARE_EXCEPTION("Invalid as a path name");
      break;
    case FR_DENIED:
      PREPARE_EXCEPTION("Permission denied");
      break;
    case FR_EXIST:
      PREPARE_EXCEPTION("File exists");
      break;
    case FR_INVALID_OBJECT:
      PREPARE_EXCEPTION("Invalid object");
      break;
    case FR_WRITE_PROTECTED:
      PREPARE_EXCEPTION("Write protected");
      break;
    case FR_INVALID_DRIVE:
      PREPARE_EXCEPTION("Invalid drive number");
      break;
    case FR_NOT_ENABLED:
      PREPARE_EXCEPTION("Drive not mouted");
      break;
    case FR_NO_FILESYSTEM:
      PREPARE_EXCEPTION("No valid FAT volume found");
      break;
    case FR_MKFS_ABORTED:
      PREPARE_EXCEPTION("Volume format could not start");
      break;
    case FR_TIMEOUT:
      PREPARE_EXCEPTION("Timeout");
      break;
    case FR_LOCKED:
      PREPARE_EXCEPTION("Object locked");
      break;
    case FR_NOT_ENOUGH_CORE:
      PREPARE_EXCEPTION("No enough memory");
      break;
    case FR_TOO_MANY_OPEN_FILES:
      PREPARE_EXCEPTION("Too many open files");
      break;
    case FR_INVALID_PARAMETER:
      PREPARE_EXCEPTION("Invalid parameter");
      break;
    default:
      PREPARE_EXCEPTION("This should not happen");
  }
  mrbc_raise(vm, MRBC_CLASS(RuntimeError), buff);
}

void
mrbc_fatfs_init(void)
{
  mrbc_class *class_FAT = mrbc_define_class(0, "FAT", mrbc_class_object);
  mrbc_define_method(0, class_FAT, "_mount", c__mount);
  mrbc_define_method(0, class_FAT, "_unmount", c__unmount);
  mrbc_define_method(0, class_FAT, "_chdir", c__chdir);
  mrbc_define_method(0, class_FAT, "_mkdir", c__mkdir);
  mrbc_define_method(0, class_FAT, "_unlink", c__unlink);
  mrbc_define_method(0, class_FAT, "_exist?", c__exist_eq);
  mrbc_define_method(0, class_FAT, "_directory?", c__directory_eq);
  mrbc_init_class_FAT_Dir();
  mrbc_init_class_FAT_File();
}

