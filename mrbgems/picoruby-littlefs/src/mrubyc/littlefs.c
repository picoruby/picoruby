static void
c_unixtime_offset_eq(struct VM *vm, mrbc_value v[], int argc)
{
  unixtime_offset = (time_t)GET_INT_ARG(1);
  SET_INT_RETURN((mrbc_int_t)unixtime_offset);
}

static void
c__erase(struct VM *vm, mrbc_value v[], int argc)
{
  if (lfs_mounted) {
    lfs_unmount(&lfs);
    lfs_mounted = false;
  }
  littlefs_hal_erase_all();
  SET_INT_RETURN(0);
}

static void
c__mkfs(struct VM *vm, mrbc_value v[], int argc)
{
  if (lfs_mounted) {
    lfs_unmount(&lfs);
    lfs_mounted = false;
  }
  littlefs_hal_init_config(&lfs_cfg);
  int err = lfs_format(&lfs, &lfs_cfg);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_format");
  SET_INT_RETURN(0);
}

static void
c_getfree(struct VM *vm, mrbc_value v[], int argc)
{
  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");
  lfs_ssize_t used = lfs_fs_size(&lfs);
  if (used < 0) {
    mrbc_raise_iff_lfs_error(vm, (int)used, "lfs_fs_size");
    return;
  }
  mrbc_int_t total = (mrbc_int_t)lfs_cfg.block_count;
  mrbc_int_t free_blocks = total - (mrbc_int_t)used;
  if (free_blocks < 0) free_blocks = 0;
  SET_INT_RETURN((total << 16) | (free_blocks & 0xFFFF));
}

static void
c__mount(struct VM *vm, mrbc_value v[], int argc)
{
  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");
  SET_INT_RETURN(0);
}

static void
c__unmount(struct VM *vm, mrbc_value v[], int argc)
{
  /* no-op */
  SET_INT_RETURN(0);
}

static void
c__chdir(struct VM *vm, mrbc_value v[], int argc)
{
  const char *path = (const char *)GET_STRING_ARG(1);
  if (path[0] != '\0') {
    int err = littlefs_ensure_mounted();
    mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");
    struct lfs_info info;
    err = lfs_stat(&lfs, path, &info);
    mrbc_raise_iff_lfs_error(vm, err, "lfs_stat");
    if (info.type != LFS_TYPE_DIR) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Not a directory");
      return;
    }
  }
  SET_INT_RETURN(0);
}

static void
c__utime(struct VM *vm, mrbc_value v[], int argc)
{
  const time_t unixtime = GET_INT_ARG(1);
  const char *path = (const char *)GET_STRING_ARG(2);
  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");
  uint32_t ts = (uint32_t)unixtime;
  err = lfs_setattr(&lfs, path, LFS_ATTR_MTIME, &ts, sizeof(ts));
  mrbc_raise_iff_lfs_error(vm, err, "lfs_setattr");
  SET_INT_RETURN(1);
}

static void
c__mkdir(struct VM *vm, mrbc_value v[], int argc)
{
  const char *path = (const char *)GET_STRING_ARG(1);
  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");
  err = lfs_mkdir(&lfs, path);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mkdir");
  SET_INT_RETURN(0);
}

static void
c__chmod(mrbc_vm *vm, mrbc_value v[], int argc)
{
  /* no-op */
  SET_INT_RETURN(0);
}

static void
c__stat(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *path = (const char *)GET_STRING_ARG(1);
  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");
  struct lfs_info info;
  err = lfs_stat(&lfs, path, &info);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_stat");

  mrbc_value stat = mrbc_hash_new(vm, 3);

  uint32_t mtime = 0;
  lfs_getattr(&lfs, path, LFS_ATTR_MTIME, &mtime, sizeof(mtime));

  mrbc_hash_set(
    &stat,
    &mrbc_symbol_value(mrbc_str_to_symid("size")),
    &mrbc_integer_value(info.size)
  );
  mrbc_hash_set(
    &stat,
    &mrbc_symbol_value(mrbc_str_to_symid("unixtime")),
    &mrbc_integer_value((mrbc_int_t)mtime)
  );
  mrbc_hash_set(
    &stat,
    &mrbc_symbol_value(mrbc_str_to_symid("mode")),
    &mrbc_integer_value(info.type)
  );
  SET_RETURN(stat);
}

static void
c__directory_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *path = (const char *)GET_STRING_ARG(1);
  int err = littlefs_ensure_mounted();
  if (err < 0) { SET_FALSE_RETURN(); return; }
  struct lfs_info info;
  err = lfs_stat(&lfs, path, &info);
  if (err == LFS_ERR_OK && info.type == LFS_TYPE_DIR) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c__setlabel(mrbc_vm *vm, mrbc_value v[], int argc)
{
  /* no-op */
  SET_INT_RETURN(0);
}

static void
c__getlabel(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value label_val = mrbc_string_new_cstr(vm, "");
  SET_RETURN(label_val);
}

static void
c__contiguous_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  SET_FALSE_RETURN();
}

void
mrbc_raise_iff_lfs_error(mrbc_vm *vm, int err, const char *func)
{
  char buff[64];
  if (LFS_prepare_exception(err, buff, func) < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), buff);
  }
}

void
c__exist_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *path = (const char *)GET_STRING_ARG(1);
  int err = littlefs_ensure_mounted();
  if (err < 0) { SET_FALSE_RETURN(); return; }
  struct lfs_info info;
  err = lfs_stat(&lfs, path, &info);
  if (err == LFS_ERR_OK) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
c__unlink(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *path = (const char *)GET_STRING_ARG(1);
  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");
  err = lfs_remove(&lfs, path);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_remove");
  SET_INT_RETURN(0);
}

void
c__rename(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *from = (const char *)GET_STRING_ARG(1);
  const char *to = (const char *)GET_STRING_ARG(2);
  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");
  err = lfs_rename(&lfs, from, to);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_rename");
  SET_INT_RETURN(0);
}

void
mrbc_littlefs_init(mrbc_vm *vm)
{
  mrbc_class *class_LFS = mrbc_define_class(vm, "Littlefs", mrbc_class_object);
  mrbc_define_method(vm, class_LFS, "unixtime_offset=", c_unixtime_offset_eq);
  mrbc_define_method(vm, class_LFS, "_erase", c__erase);
  mrbc_define_method(vm, class_LFS, "_mkfs", c__mkfs);
  mrbc_define_method(vm, class_LFS, "getfree", c_getfree);
  mrbc_define_method(vm, class_LFS, "_mount", c__mount);
  mrbc_define_method(vm, class_LFS, "_unmount", c__unmount);
  mrbc_define_method(vm, class_LFS, "_chdir", c__chdir);
  mrbc_define_method(vm, class_LFS, "_utime", c__utime);
  mrbc_define_method(vm, class_LFS, "_mkdir", c__mkdir);
  mrbc_define_method(vm, class_LFS, "_unlink", c__unlink);
  mrbc_define_method(vm, class_LFS, "_rename", c__rename);
  mrbc_define_method(vm, class_LFS, "_chmod", c__chmod);
  mrbc_define_method(vm, class_LFS, "_exist?", c__exist_q);
  mrbc_define_method(vm, class_LFS, "_directory?", c__directory_q);
  mrbc_define_method(vm, class_LFS, "_setlabel", c__setlabel);
  mrbc_define_method(vm, class_LFS, "_getlabel", c__getlabel);
  mrbc_define_method(vm, class_LFS, "_contiguous?", c__contiguous_q);
  mrbc_init_class_Littlefs_Dir(vm, class_LFS);
  mrbc_init_class_Littlefs_File(vm, class_LFS);

  mrbc_class *class_LFS_Stat = mrbc_define_class_under(vm, class_LFS, "Stat", mrbc_class_object);
  mrbc_define_method(vm, class_LFS_Stat, "_stat", c__stat);
}
