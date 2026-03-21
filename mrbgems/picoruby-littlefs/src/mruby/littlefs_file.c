#include <string.h>
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/class.h"
#include "mruby/data.h"

static void
mrb_lfs_file_free(mrb_state *mrb, void *ptr)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)ptr;
  if (fd) {
    lfs_file_close(littlefs_get_lfs(), &fd->file);
    mrb_free(mrb, fd);
  }
}

struct mrb_data_type mrb_lfs_file_type = {
  "LittlefsFile", mrb_lfs_file_free,
};

static mrb_value
mrb_s_new(mrb_state *mrb, mrb_value klass)
{
  const char *path;
  const char *mode_str;
  mrb_get_args(mrb, "zz", &path, &mode_str);

  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");

  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_malloc(mrb, sizeof(lfs_file_data_t));
  mrb_value file = mrb_obj_value(
    Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_lfs_file_type, fd));

  int flags = 0;
  fd->writable = false;
  if (strcmp(mode_str, "r") == 0) {
    flags = LFS_O_RDONLY;
  } else if (strcmp(mode_str, "r+") == 0) {
    flags = LFS_O_RDWR;
    fd->writable = true;
  } else if (strcmp(mode_str, "w") == 0) {
    flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC;
    fd->writable = true;
  } else if (strcmp(mode_str, "w+") == 0) {
    flags = LFS_O_RDWR | LFS_O_CREAT | LFS_O_TRUNC;
    fd->writable = true;
  } else if (strcmp(mode_str, "a") == 0) {
    flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND;
    fd->writable = true;
  } else if (strcmp(mode_str, "a+") == 0) {
    flags = LFS_O_RDWR | LFS_O_CREAT | LFS_O_APPEND;
    fd->writable = true;
  } else if (strcmp(mode_str, "wx") == 0) {
    flags = LFS_O_WRONLY | LFS_O_CREAT | LFS_O_EXCL;
    fd->writable = true;
  } else if (strcmp(mode_str, "w+x") == 0) {
    flags = LFS_O_RDWR | LFS_O_CREAT | LFS_O_EXCL;
    fd->writable = true;
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Unknown file open mode");
  }

  strncpy(fd->path, path, LFS_NAME_MAX);
  fd->path[LFS_NAME_MAX] = '\0';

  err = lfs_file_open(littlefs_get_lfs(), &fd->file, path, flags);
  if (err != LFS_ERR_OK) {
    mrb_free(mrb, fd);
    mrb_data_init(file, NULL, &mrb_lfs_file_type);
    mrb_raise_iff_lfs_error(mrb, err, path);
  }
  return file;
}

static mrb_value
mrb_sector_size(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(littlefs_get_config()->block_size);
}

static mrb_value
mrb_tell(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  lfs_soff_t pos = lfs_file_tell(littlefs_get_lfs(), &fd->file);
  if (pos < 0) {
    mrb_raise_iff_lfs_error(mrb, (int)pos, "lfs_file_tell");
  }
  return mrb_fixnum_value(pos);
}

static mrb_value
mrb_seek(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  mrb_int ofs;
  mrb_int whence;
  mrb_get_args(mrb, "ii", &ofs, &whence);

  int lfs_whence;
  if (whence == SEEK_SET) {
    lfs_whence = LFS_SEEK_SET;
  } else if (whence == SEEK_CUR) {
    lfs_whence = LFS_SEEK_CUR;
  } else if (whence == SEEK_END) {
    lfs_whence = LFS_SEEK_END;
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Unknown whence");
    return mrb_nil_value();
  }

  lfs_soff_t new_pos = lfs_file_seek(littlefs_get_lfs(), &fd->file, (lfs_soff_t)ofs, lfs_whence);
  if (new_pos < 0) {
    mrb_raise_iff_lfs_error(mrb, (int)new_pos, "lfs_file_seek");
  }
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_size(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  lfs_soff_t sz = lfs_file_size(littlefs_get_lfs(), &fd->file);
  if (sz < 0) {
    mrb_raise_iff_lfs_error(mrb, (int)sz, "lfs_file_size");
  }
  return mrb_fixnum_value(sz);
}

static mrb_value
mrb_eof_p(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  lfs_soff_t pos = lfs_file_tell(littlefs_get_lfs(), &fd->file);
  lfs_soff_t sz = lfs_file_size(littlefs_get_lfs(), &fd->file);
  if (pos < 0 || sz < 0) return mrb_true_value();
  if (sz <= pos) {
    return mrb_true_value();
  }
  return mrb_false_value();
}

static mrb_value
mrb_read(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  mrb_int btr;
  mrb_get_args(mrb, "i", &btr);
  char buff[btr];
  lfs_ssize_t br = lfs_file_read(littlefs_get_lfs(), &fd->file, buff, (lfs_size_t)btr);
  if (br < 0) {
    mrb_raise_iff_lfs_error(mrb, (int)br, "lfs_file_read");
  }
  if (0 < br) {
    return mrb_str_new(mrb, (const void *)buff, br);
  }
  return mrb_nil_value();
}

static mrb_value
mrb_getbyte(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  char buff[1];
  lfs_ssize_t br = lfs_file_read(littlefs_get_lfs(), &fd->file, buff, 1);
  if (br == 1) {
    return mrb_fixnum_value((unsigned char)buff[0]);
  }
  return mrb_nil_value();
}

static mrb_value
mrb_write(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  mrb_value str;
  mrb_get_args(mrb, "S", &str);
  lfs_ssize_t bw = lfs_file_write(
    littlefs_get_lfs(), &fd->file, RSTRING_PTR(str), RSTRING_LEN(str));
  if (bw < 0) {
    mrb_raise_iff_lfs_error(mrb, (int)bw, "lfs_file_write");
  }
  int err = lfs_file_sync(littlefs_get_lfs(), &fd->file);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_file_sync");
  return mrb_fixnum_value(bw);
}

static mrb_value
mrb_File_close(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  if (!fd) return mrb_nil_value();
  bool writable = fd->writable;
  char path[LFS_NAME_MAX + 1];
  strncpy(path, fd->path, LFS_NAME_MAX);
  path[LFS_NAME_MAX] = '\0';
  int err = lfs_file_close(littlefs_get_lfs(), &fd->file);
  mrb_free(mrb, fd);
  mrb_data_init(self, NULL, &mrb_lfs_file_type);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_file_close");
  if (writable) {
    uint32_t ts = littlefs_get_unixtime();
    lfs_setattr(littlefs_get_lfs(), path, LFS_ATTR_MTIME, &ts, sizeof(ts));
  }
  return mrb_nil_value();
}

static mrb_value
mrb_expand(mrb_state *mrb, mrb_value self)
{
  /* no-op: littlefs does not support pre-allocation */
  mrb_int size;
  mrb_get_args(mrb, "i", &size);
  return mrb_fixnum_value(size);
}

static mrb_value
mrb_fsync(mrb_state *mrb, mrb_value self)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_file_type);
  int err = lfs_file_sync(littlefs_get_lfs(), &fd->file);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_file_sync");
  return mrb_fixnum_value(0);
}

static void
mrb_vfs_methods_free(mrb_state *mrb, void *ptr)
{
}

struct mrb_data_type mrb_vfs_methods_type = {
  "VFSMethods", mrb_vfs_methods_free,
};

static mrb_value
mrb_s_vfs_methods(mrb_state *mrb, mrb_value klass)
{
  prb_vfs_methods m = {
    mrb_s_new,
    mrb_File_close,
    mrb_read,
    mrb_getbyte,
    mrb_write,
    mrb_seek,
    mrb_tell,
    mrb_size,
    mrb_fsync,
    mrb__exist_p,
    mrb__unlink
  };
  prb_vfs_methods *mm = (prb_vfs_methods *)mrb_malloc(mrb, sizeof(prb_vfs_methods));
  memcpy(mm, &m, sizeof(prb_vfs_methods));
  mrb_value methods = mrb_obj_value(
    Data_Wrap_Struct(mrb, mrb_class(mrb, klass), &mrb_vfs_methods_type, mm));
  return methods;
}

void
mrb_init_class_Littlefs_File(mrb_state *mrb, struct RClass *class_LFS)
{
  struct RClass *class_LFS_File = mrb_define_class_under_id(
    mrb, class_LFS, MRB_SYM(File), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_LFS_File, MRB_TT_CDATA);

  struct RClass *class_LFS_VFSMethods = mrb_define_class_under_id(
    mrb, class_LFS, MRB_SYM(VFSMethods), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_LFS_VFSMethods, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_LFS_File, MRB_SYM(new), mrb_s_new, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_LFS_File, MRB_SYM(open), mrb_s_new, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(tell), mrb_tell, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(seek), mrb_seek, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM_Q(eof), mrb_eof_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(read), mrb_read, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(getbyte), mrb_getbyte, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(write), mrb_write, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(close), mrb_File_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(size), mrb_size, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(expand), mrb_expand, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(fsync), mrb_fsync, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_File, MRB_SYM(sector_size), mrb_sector_size, MRB_ARGS_NONE());

  mrb_define_class_method_id(mrb, class_LFS, MRB_SYM(vfs_methods), mrb_s_vfs_methods, MRB_ARGS_NONE());
}
