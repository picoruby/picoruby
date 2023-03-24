#include <stdio.h>
#include <stdint.h>

#include "../include/fat.h"

#include "../lib/ff14b/source/ff.h"

static void
c_new(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FRESULT res;
  FILINFO fno;
  const TCHAR *path = (const TCHAR *)GET_STRING_ARG(1);

  res = f_stat(path, &fno);
  if (res != FR_INVALID_NAME) {
    /* FIXME: pathname "0:" becomes INVALID, why? */
    mrbc_raise_iff_f_error(vm, res, "f_stat");
  }
  if (res == FR_OK && (fno.fattrib & AM_DIR) == 0) {
    mrbc_raise(
      vm, MRBC_CLASS(RuntimeError), // Errno::ENOTDIR in CRuby
      "Not a directory @ dir_initialize"
    );
    return;
  }

  mrbc_value dir = mrbc_instance_new(vm, v->cls, sizeof(DIR));
  DIR *dp = (DIR *)dir.instance->data;
  res = f_opendir(dp, path);
  mrbc_raise_iff_f_error(vm, res, "f_opendir");
  SET_RETURN(dir);
}

static void
c_close(struct VM *vm, mrbc_value v[], int argc)
{
  DIR *dp = (DIR *)v->instance->data;
  FRESULT res = f_closedir(dp);
  mrbc_raise_iff_f_error(vm, res, "f_close");
  SET_NIL_RETURN();
}

static void
c_findnext(struct VM *vm, mrbc_value v[], int argc)
{
  DIR *dp = (DIR *)v->instance->data;
  FRESULT fr;
  FILINFO fno;
  fr = f_findnext(dp, &fno);
  if (fr == FR_OK && fno.fname[0]) {
    mrbc_value value = mrbc_string_new_cstr(vm, (const char *)(fno.fname));
    SET_RETURN(value);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_pat_eq(struct VM *vm, mrbc_value v[], int argc)
{
  DIR *dp = (DIR *)v->instance->data;
  dp->pat = (const TCHAR *)GET_STRING_ARG(1);
  SET_INT_RETURN(0);
}

static void
c_read(struct VM *vm, mrbc_value v[], int argc)
{
  DIR *dp = (DIR *)v->instance->data;
  FILINFO fno;
  FRESULT res = f_readdir(dp, &fno);
  mrbc_raise_iff_f_error(vm, res, "f_readdir");
  if (fno.fname[0] == 0) {
    SET_NIL_RETURN();
  } else {
    mrbc_value value = mrbc_string_new_cstr(vm, (const char *)(fno.fname));
    SET_RETURN(value);
  }
}

static void
c_rewind(struct VM *vm, mrbc_value v[], int argc)
{
  DIR *dp = (DIR *)v->instance->data;
  f_rewinddir(dp);
}

void
mrbc_init_class_FAT_Dir(void)
{
  mrbc_class *class_FAT = mrbc_define_class(0, "FAT", mrbc_class_object);

  mrbc_sym symid = mrbc_search_symid("Dir");
  mrbc_value *v = mrbc_get_class_const(class_FAT, symid);
  mrbc_class *class_FAT_Dir = v->cls;

  mrbc_define_method(0, class_FAT_Dir, "new", c_new);
  mrbc_define_method(0, class_FAT_Dir, "close", c_close);
  mrbc_define_method(0, class_FAT_Dir, "read", c_read);
  mrbc_define_method(0, class_FAT_Dir, "rewind", c_rewind);
  mrbc_define_method(0, class_FAT_Dir, "pat=", c_pat_eq);
  mrbc_define_method(0, class_FAT_Dir, "findnext", c_findnext);
}

