/* Simple glob pattern match supporting '*' and '?' */
static int
simple_match(const char *pat, const char *str)
{
  while (*pat && *str) {
    if (*pat == '*') {
      pat++;
      if (*pat == '\0') return 1;
      while (*str) {
        if (simple_match(pat, str)) return 1;
        str++;
      }
      return 0;
    } else if (*pat == '?' || *pat == *str) {
      pat++;
      str++;
    } else {
      return 0;
    }
  }
  while (*pat == '*') pat++;
  return (*pat == '\0' && *str == '\0');
}

static void
c_new(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *path = (const char *)GET_STRING_ARG(1);

  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");

  struct lfs_info info;
  err = lfs_stat(littlefs_get_lfs(), path, &info);
  if (err == LFS_ERR_OK && info.type != LFS_TYPE_DIR) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Not a directory @ dir_initialize");
    return;
  }

  mrbc_value dir = mrbc_instance_new(vm, v->cls, sizeof(lfs_dir_t));
  lfs_dir_t *dp = (lfs_dir_t *)dir.instance->data;
  err = lfs_dir_open(littlefs_get_lfs(), dp, path);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_dir_open");
  SET_RETURN(dir);
}

static void
c_close(struct VM *vm, mrbc_value v[], int argc)
{
  lfs_dir_t *dp = (lfs_dir_t *)v->instance->data;
  int err = lfs_dir_close(littlefs_get_lfs(), dp);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_dir_close");
  SET_NIL_RETURN();
}

static void
c_findnext(struct VM *vm, mrbc_value v[], int argc)
{
  lfs_dir_t *dp = (lfs_dir_t *)v->instance->data;
  mrbc_value pat_val = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("pat"));
  const char *pat = (pat_val.tt == MRBC_TT_NIL) ? "*" : (const char *)pat_val.string->data;

  struct lfs_info info;
  int res;
  while ((res = lfs_dir_read(littlefs_get_lfs(), dp, &info)) > 0) {
    if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
      continue;
    }
    if (simple_match(pat, info.name)) {
      mrbc_value value = mrbc_string_new_cstr(vm, info.name);
      SET_RETURN(value);
      return;
    }
  }
  SET_NIL_RETURN();
}

static void
c_pat_eq(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value pat = v[1];
  mrbc_instance_setiv(&v[0], mrbc_str_to_symid("pat"), &pat);
  SET_INT_RETURN(0);
}

static void
c_read(struct VM *vm, mrbc_value v[], int argc)
{
  lfs_dir_t *dp = (lfs_dir_t *)v->instance->data;
  struct lfs_info info;
  int res;
  while ((res = lfs_dir_read(littlefs_get_lfs(), dp, &info)) > 0) {
    if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
      continue;
    }
    mrbc_value value = mrbc_string_new_cstr(vm, info.name);
    SET_RETURN(value);
    return;
  }
  SET_NIL_RETURN();
}

static void
c_rewind(struct VM *vm, mrbc_value v[], int argc)
{
  lfs_dir_t *dp = (lfs_dir_t *)v->instance->data;
  int err = lfs_dir_rewind(littlefs_get_lfs(), dp);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_dir_rewind");
}

void
mrbc_init_class_Littlefs_Dir(mrbc_vm *vm, mrbc_class *class_LFS)
{
  mrbc_class *class_LFS_Dir = mrbc_define_class_under(vm, class_LFS, "Dir", mrbc_class_object);

  mrbc_define_method(vm, class_LFS_Dir, "new", c_new);
  mrbc_define_method(vm, class_LFS_Dir, "close", c_close);
  mrbc_define_method(vm, class_LFS_Dir, "read", c_read);
  mrbc_define_method(vm, class_LFS_Dir, "rewind", c_rewind);
  mrbc_define_method(vm, class_LFS_Dir, "pat=", c_pat_eq);
  mrbc_define_method(vm, class_LFS_Dir, "findnext", c_findnext);
}
