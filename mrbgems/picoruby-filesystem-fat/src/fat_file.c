#include <stdio.h>
#include <stdint.h>

#include "../include/fat.h"

#include "../lib/ff14b/source/ff.h"

static void
c_new(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FRESULT res;
  const TCHAR *path = (const TCHAR *)GET_STRING_ARG(1);

  mrbc_value _file = mrbc_instance_new(vm, v->cls, sizeof(FIL));
  FIL *fp = (FIL *)_file.instance->data;
  BYTE mode = 0;
  const char *mode_str = (const char *)GET_STRING_ARG(2);
  if (strcmp(mode_str, "r") == 0) {
    mode = FA_READ;
  } else if (strcmp(mode_str, "r+") == 0) {
    mode = FA_READ | FA_WRITE;
  } else if (strcmp(mode_str, "w") == 0) {
    mode = FA_CREATE_ALWAYS | FA_WRITE;
  } else if (strcmp(mode_str, "w+") == 0) {
    mode = FA_CREATE_ALWAYS | FA_WRITE | FA_READ;
  } else if (strcmp(mode_str, "a") == 0) {
    mode = FA_OPEN_APPEND | FA_WRITE;
  } else if (strcmp(mode_str, "a+") == 0) {
    mode = FA_OPEN_APPEND | FA_WRITE | FA_READ;
  } else if (strcmp(mode_str, "wx") == 0) {
    mode = FA_CREATE_NEW | FA_WRITE;
  } else if (strcmp(mode_str, "w+x") == 0) {
    mode = FA_CREATE_NEW | FA_WRITE | FA_READ;
  } else {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Unknown file open mode");
  }
  res = f_open(fp, path, mode);
  mrbc_raise_iff_f_error(vm, res, "f_open");
  SET_RETURN(_file);
}

static void
c_tell(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  FSIZE_t pos = f_tell(fp);
  SET_INT_RETURN(pos);
}

static void
c_seek(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  int ofs = GET_INT_ARG(1);
  int whence = GET_INT_ARG(2);
  FSIZE_t size = f_size(fp);
  if (whence == SEEK_SET) {
    // do nothing
  } else if (whence == SEEK_CUR) {
    ofs = f_tell(fp) + ofs;
  } else if (whence == SEEK_END) {
    ofs = size + ofs;
  } else {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Unknown whence");
    return;
  }
  if (ofs < 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Invalid offset");
    return;
  } else if (size < ofs) {
    ofs = size;
  }
  FRESULT res;
  res = f_lseek(fp, (FSIZE_t)ofs);
  mrbc_raise_iff_f_error(vm, res, "f_lseek");
  SET_INT_RETURN(0);
}

static void
c_size(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  SET_INT_RETURN(f_size(fp));
}


static void
c_eof_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  if (f_eof(fp) == 0) {
    SET_FALSE_RETURN();
  } else {
    SET_TRUE_RETURN();
  }
}

static void
c_read(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  UINT btr = GET_INT_ARG(1);
  char buff[btr];
  UINT br;
  FRESULT res = f_read(fp, buff, btr, &br);
  mrbc_raise_iff_f_error(vm, res, "f_read");
  if (0 < br) {
    mrbc_value value = mrbc_string_new(vm, (const void *)buff, br);
    SET_RETURN(value);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_write(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  mrbc_value str = v[1];
  UINT bw;
  FRESULT res;
  res = f_write(fp, str.string->data, str.string->size, &bw);
  mrbc_raise_iff_f_error(vm, res, "f_write");
  SET_INT_RETURN(bw);
}

static void
c_close(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  FRESULT res;
  res = f_close(fp);
  mrbc_raise_iff_f_error(vm, res, "f_close");
  SET_NIL_RETURN();
}

static void
c_expand(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  FRESULT res;
  FSIZE_t size = GET_INT_ARG(1);
  res = f_expand(fp, size, 1);
  mrbc_raise_iff_f_error(vm, res, "f_expand");
  SET_INT_RETURN(size);
}

static void
c_fsync(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FIL *fp = (FIL *)v->instance->data;
  FRESULT res;
  res = f_sync(fp);
  mrbc_raise_iff_f_error(vm, res, "f_sync");
  SET_INT_RETURN(0);
}

static void
c_vfs_methods(mrbc_vm *vm, mrbc_value v[], int argc)
{
   prb_vfs_methods m = {
    c_new,
    c_close,
    c_read,
    c_write,
    c_seek,
    c_tell,
    c_size,
    c_fsync,
    c__exist_q,
    c__unlink
  };
  mrbc_value methods = mrbc_instance_new(vm, v->cls, sizeof(prb_vfs_methods));
  memcpy(methods.instance->data, &m, sizeof(prb_vfs_methods));
  SET_RETURN(methods);
}

void
mrbc_init_class_FAT_File(void)
{
  mrbc_class *class_FAT = mrbc_define_class(0, "FAT", mrbc_class_object);

  mrbc_sym symid = mrbc_search_symid("File");
  mrbc_value *v = mrbc_get_class_const(class_FAT, symid);
  mrbc_class *class_FAT_File = v->cls;

  mrbc_define_method(0, class_FAT_File, "new", c_new);
  mrbc_define_method(0, class_FAT_File, "open", c_new);
  mrbc_define_method(0, class_FAT_File, "tell", c_tell);
  mrbc_define_method(0, class_FAT_File, "seek", c_seek);
  mrbc_define_method(0, class_FAT_File, "eof?", c_eof_q);
  mrbc_define_method(0, class_FAT_File, "read", c_read);
  mrbc_define_method(0, class_FAT_File, "write", c_write);
  mrbc_define_method(0, class_FAT_File, "close", c_close);
  mrbc_define_method(0, class_FAT_File, "size", c_size);
  mrbc_define_method(0, class_FAT_File, "expand", c_expand);
  mrbc_define_method(0, class_FAT_File, "fsync", c_fsync);

  mrbc_define_method(0, class_FAT_File, "vfs_methods", c_vfs_methods);
}

