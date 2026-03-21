#include <string.h>
#include "mruby/data.h"
#include "mruby/class.h"
#include "mruby/hash.h"
#include "mruby/string.h"
#include "mruby/presym.h"
#include "mruby/variable.h"

static void
mrb_littlefs_free(mrb_state *mrb, void *ptr)
{
  (void)mrb;
  (void)ptr;
  /* littlefs uses a global instance; nothing to free per object */
}

struct mrb_data_type mrb_littlefs_type = {
  "Littlefs", mrb_littlefs_free
};

void
mrb_raise_iff_lfs_error(mrb_state *mrb, int err, const char *func)
{
  char buff[64];
  if (LFS_prepare_exception(err, buff, func) < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, buff);
  }
}

static mrb_value
mrb_unixtime_offset_e(mrb_state *mrb, mrb_value klass)
{
  mrb_int offset;
  mrb_get_args(mrb, "i", &offset);
  unixtime_offset = (time_t)offset;
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__erase(mrb_state *mrb, mrb_value self)
{
  if (lfs_mounted) {
    lfs_unmount(&lfs);
    lfs_mounted = false;
  }
  littlefs_hal_erase_all();
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__mkfs(mrb_state *mrb, mrb_value self)
{
  if (lfs_mounted) {
    lfs_unmount(&lfs);
    lfs_mounted = false;
  }
  littlefs_hal_init_config(&lfs_cfg);
  int err = lfs_format(&lfs, &lfs_cfg);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_format");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_getfree(mrb_state *mrb, mrb_value self)
{
  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");
  lfs_ssize_t used = lfs_fs_size(&lfs);
  if (used < 0) {
    mrb_raise_iff_lfs_error(mrb, (int)used, "lfs_fs_size");
  }
  mrb_int total = (mrb_int)lfs_cfg.block_count;
  mrb_int free_blocks = total - (mrb_int)used;
  if (free_blocks < 0) free_blocks = 0;
  return mrb_fixnum_value((total << 16) | (free_blocks & 0xFFFF));
}

static mrb_value
mrb__mount(mrb_state *mrb, mrb_value self)
{
  DATA_PTR(self) = &lfs;
  DATA_TYPE(self) = &mrb_littlefs_type;
  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__unmount(mrb_state *mrb, mrb_value self)
{
  /* no-op: keep filesystem mounted */
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__chdir(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  /* littlefs has no chdir; VFS layer manages cwd.
   * Just verify the path exists if non-empty. */
  if (path[0] != '\0') {
    int err = littlefs_ensure_mounted();
    mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");
    struct lfs_info info;
    err = lfs_stat(&lfs, path, &info);
    mrb_raise_iff_lfs_error(mrb, err, "lfs_stat");
    if (info.type != LFS_TYPE_DIR) {
      mrb_raise(mrb, E_RUNTIME_ERROR, "Not a directory");
    }
  }
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__utime(mrb_state *mrb, mrb_value self)
{
  mrb_int unixtime;
  const char *path;
  mrb_get_args(mrb, "iz", &unixtime, &path);
  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");
  uint32_t ts = (uint32_t)unixtime;
  err = lfs_setattr(&lfs, path, LFS_ATTR_MTIME, &ts, sizeof(ts));
  mrb_raise_iff_lfs_error(mrb, err, "lfs_setattr");
  return mrb_fixnum_value(1);
}

static mrb_value
mrb__mkdir(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_int mode = 0;
  mrb_get_args(mrb, "z|i", &path, &mode);
  (void)mode;
  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");
  err = lfs_mkdir(&lfs, path);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mkdir");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__chmod(mrb_state *mrb, mrb_value self)
{
  /* no-op: littlefs has no permission attributes */
  mrb_int attr;
  const char *path;
  mrb_get_args(mrb, "iz", &attr, &path);
  (void)attr;
  (void)path;
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__stat(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");
  struct lfs_info info;
  err = lfs_stat(&lfs, path, &info);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_stat");

  mrb_value stat = mrb_hash_new_capa(mrb, 3);

  /* Try to read mtime from custom attribute */
  uint32_t mtime = 0;
  lfs_getattr(&lfs, path, LFS_ATTR_MTIME, &mtime, sizeof(mtime));

  mrb_hash_set(mrb, stat,
    mrb_symbol_value(MRB_SYM(size)),
    mrb_int_value(mrb, info.type == LFS_TYPE_DIR ? 0 : (mrb_int)info.size));
  mrb_hash_set(mrb, stat,
    mrb_symbol_value(MRB_SYM(unixtime)),
    mrb_int_value(mrb, (mrb_int)mtime));
  mrb_hash_set(mrb, stat,
    mrb_symbol_value(MRB_SYM(mode)),
    mrb_fixnum_value(info.type));
  return stat;
}

static mrb_value
mrb__directory_p(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  int err = littlefs_ensure_mounted();
  if (err < 0) return mrb_false_value();
  struct lfs_info info;
  err = lfs_stat(&lfs, path, &info);
  if (err == LFS_ERR_OK && info.type == LFS_TYPE_DIR) {
    return mrb_true_value();
  }
  return mrb_false_value();
}

static mrb_value
mrb__setlabel(mrb_state *mrb, mrb_value self)
{
  /* no-op: littlefs has no volume label concept */
  const char *label;
  mrb_get_args(mrb, "z", &label);
  (void)label;
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__getlabel(mrb_state *mrb, mrb_value self)
{
  /* Return empty string; label is managed in Ruby layer */
  return mrb_str_new_cstr(mrb, "");
}

static mrb_value
mrb__contiguous_p(mrb_state *mrb, mrb_value self)
{
  /* littlefs does not guarantee contiguous allocation */
  const char *path;
  mrb_get_args(mrb, "z", &path);
  (void)path;
  return mrb_false_value();
}

mrb_value
mrb__exist_p(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  int err = littlefs_ensure_mounted();
  if (err < 0) return mrb_false_value();
  struct lfs_info info;
  err = lfs_stat(&lfs, path, &info);
  if (err == LFS_ERR_OK) {
    return mrb_true_value();
  }
  return mrb_false_value();
}

mrb_value
mrb__unlink(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");
  err = lfs_remove(&lfs, path);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_remove");
  return mrb_fixnum_value(0);
}

mrb_value
mrb__rename(mrb_state *mrb, mrb_value self)
{
  const char *from, *to;
  mrb_get_args(mrb, "zz", &from, &to);
  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");
  err = lfs_rename(&lfs, from, to);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_rename");
  return mrb_fixnum_value(0);
}

void
mrb_picoruby_littlefs_gem_init(mrb_state *mrb)
{
  struct RClass *class_LFS = mrb_define_class_id(mrb, MRB_SYM(Littlefs), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_LFS, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_LFS, MRB_SYM_E(unixtime_offset), mrb_unixtime_offset_e, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_erase), mrb__erase, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_mkfs), mrb__mkfs, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(getfree), mrb_getfree, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_mount), mrb__mount, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_unmount), mrb__unmount, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_chdir), mrb__chdir, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_utime), mrb__utime, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_mkdir), mrb__mkdir, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_unlink), mrb__unlink, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_rename), mrb__rename, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_chmod), mrb__chmod, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM_Q(_exist), mrb__exist_p, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM_Q(_directory), mrb__directory_p, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_setlabel), mrb__setlabel, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS, MRB_SYM(_getlabel), mrb__getlabel, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS, MRB_SYM_Q(_contiguous), mrb__contiguous_p, MRB_ARGS_REQ(1));
  mrb_init_class_Littlefs_Dir(mrb, class_LFS);
  mrb_init_class_Littlefs_File(mrb, class_LFS);

  struct RClass *class_LFS_Stat = mrb_define_class_under_id(mrb, class_LFS, MRB_SYM(Stat), mrb->object_class);
  mrb_define_method_id(mrb, class_LFS_Stat, MRB_SYM(_stat), mrb__stat, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_littlefs_gem_final(mrb_state *mrb)
{
  if (lfs_mounted) {
    lfs_unmount(&lfs);
    lfs_mounted = false;
  }
}
