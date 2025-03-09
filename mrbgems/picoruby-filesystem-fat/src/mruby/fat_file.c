#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/data.h>

static void
mrb_fat_file_free(mrb_state *mrb, void *ptr) {
  f_close((FIL *)ptr);
}

struct mrb_data_type mrb_fat_file_type = {
  "FATFile", mrb_fat_file_free,
};


static mrb_value
mrb_s_new(mrb_state *mrb, mrb_value klass)
{
  FRESULT res;
  const char *path;
  const char *mode_str;
  mrb_get_args(mrb, "zz", &path, &mode_str);
  mrb_value file = mrb_instance_new(mrb, klass);
  FIL *fp = (FILE *)mrb_malloc(mrb, sizeof(FIL));
  MRB_PTR(file) = fp;
  MRB_TYPE(file) = &mrb_fat_file_type;
  BYTE mode = 0;
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
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Unknown file open mode");
  }
  res = f_open(fp, (const TCHAR *)path, mode);
  mrb_raise_iff_f_error(mrb, res, "f_open");
  return file;
}

static mrb_value
mrb_sector_size(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(FILE_sector_size());
}

static mrb_value
mrb_physical_address(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  uint8_t *addr;

  /* Check if file object is valid */
  if (!fp) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Invalid argument");
  }
  if (!fp->obj.fs) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Invalid file object");
  }

  FILE_physical_address(fp, &addr);

  /* Return physical address */
  return mrb_fixnum_value((intptr_t)addr);
}

static mrb_value
mrb_tell(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  FSIZE_t pos = f_tell(fp);
  return mrb_fixnum_value(pos);
}

static mrb_value
mrb_seek(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  int ofs = GET_INT_ARG(1);
  int whence = GET_INT_ARG(2);
  FSIZE_t size = f_size(fp);
  FSIZE_t new_pos;

  if (whence == SEEK_SET) {
    new_pos = ofs;
  } else if (whence == SEEK_CUR) {
    new_pos = f_tell(fp) + ofs;
  } else if (whence == SEEK_END) {
    new_pos = size + ofs;
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Unknown whence");
  }

  if (new_pos < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Invalid offset");
  } else if (size < new_pos) {
    FRESULT res;
    res = f_expand(fp, new_pos, 1);
    if (res == FR_OK) res = f_sync(fp);
    mrb_raise_iff_f_error(mrb, res, "f_lseek|f_expand|f_sync");
  }
  FRESULT res;
  res = f_lseek(fp, new_pos);
  mrb_raise_iff_f_error(mrb, res, "f_lseek");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_size(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  return mrb_fixnum_value(f_size(fp))
}


static mrb_value
mrb_eof_p(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  if (f_eof(fp) == 0) {
    return mrb_false_value();
  } else {
    return mrb_true_value();
  }
}

static mrb_value
mrb_read(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  mrb_int btr;
  mrb_get_args(mrb, "i", &btr);
  char buff[btr];
  UINT br;
  FRESULT res = f_read(fp, buff, (UINT)btr, &br);
  mrb_raise_iff_f_error(mrb, res, "f_read");
  if (0 < br) {
    mrb_value value = mrb_str_new(mrb, (const void *)buff, br);
    return value;
  } else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb_write(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  mrb_value str = v[1];
  UINT bw;
  FRESULT res;
  res = f_write(fp, str.string->data, str.string->size, &bw);
  if (res == FR_OK) res = f_sync(fp);
  mrb_raise_iff_f_error(mrb, res, "f_write|f_sync");
  return mrb_fixnum_value(bw);
}

static mrb_value
mrb_close(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  FRESULT res;
  res = f_close(fp);
  mrb_raise_iff_f_error(mrb, res, "f_close");
  return mrb_nil_value();
}

static mrb_value
mrb_expand(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  FRESULT res;
  mrb_int size;
  mrb_get_args(mrb, "i", &size);
  res = f_expand(fp, (FSIZE_t)size, 1);
  if (res == FR_OK) res = f_sync(fp);
  mrb_raise_iff_f_error(mrb, res, "f_expand|f_sync");
  return mrb_fixnum_value(size);
}

static mrb_value
mrb_fsync(mrb_state *mrb, mrb_value self)
{
  FIL *fp = (FIL *)mrb_data_get_ptr(mrb, self, &mrb_fat_file_type);
  FRESULT res;
  res = f_sync(fp);
  mrb_raise_iff_f_error(mrb, res, "f_sync");
  return mrb_fixnum_value(0);
}


static void
mrb_vfs_methods_free(mrb_state *mrb, void *ptr) {
}

struct mrb_data_type mrb_vfs_methods_type = {
  "VFSMethods", mrb_vfs_methods_free,
};

static mrb_value
mrb_s_vfs_methods(mrb_state *mrb, mrb_value klass)
{
  prb_vfs_methods m = {
    mrb_s_new,
    mrb_close,
    mrb_read,
    mrb_write,
    mrb_seek,
    mrb_tell,
    mrb_size,
    mrb_fsync,
    mrb__exist_q,
    mrb__unlink
  };
  mrb_value methods = mrb_instance_new(mrb, klass);
  prb_vfs_methods mm = (prb_vfs_methods *)mrb_malloc(mrb, sizeof(prb_vfs_methods));
  memcpy(mm, &m, sizeof(prb_vfs_methods));
  MRB_PTR(methods) = &mm;
  MRB_TYPE(methods) = &mrb_vfs_methods_type;
  SET_RETURN(methods);
}

void
mrb_init_class_FAT_File(mrb_vm *mrb ,struct RClass *class_FAT)
{

  struct RClass class_FAT_File = mrb_define_class_under_id(mrb, class_FAT, MRB_SYM(File), mrb->object_classs);
  MRB_SET_INSTANCE_TT(class_FAT_File, MRB_TT_CDATA);

  struct RClass class_FAT_VFSMethods = mrb_define_class_under_id(mrb, class_FAT, MRB_SYM(VFSMethods), mrb->object_classs);
  MRB_SET_INSTANCE_TT(class_FAT_VFSMethods, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_FAT_File, MRB_SYM(new), mrb_s_new, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_FAT_File, MRB_SYM(open), mrb_s_new, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(tell), mrb_tell, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(seek), mrb_seek, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM_Q(eof), mrb_eof_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(read), mrb_read, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(write), mrb_write, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(close), mrb_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(size), mrb_size, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(expand), mrb_expand, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(fsync), mrb_fsync, MRB_ARGS_NONE());

  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(physical_address), mrb_physical_address, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FAT_File, MRB_SYM(sector_size), mrb_sector_size, MRB_ARGS_NONE());

  mrb_define_class_method_id(mrb, class_FAT, MRB_SYM(vfs_methods), mrb_s_vfs_methods, MRB_ARGS_NONE());
}
