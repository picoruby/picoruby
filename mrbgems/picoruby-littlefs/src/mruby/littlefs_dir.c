#include <string.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/variable.h>

static void
mrb_lfs_dir_free(mrb_state *mrb, void *ptr)
{
  lfs_dir_t *dp = (lfs_dir_t *)ptr;
  if (dp) {
    lfs_dir_close(littlefs_get_lfs(), dp);
    mrb_free(mrb, dp);
  }
}

struct mrb_data_type mrb_lfs_dir_type = {
  "LittlefsDir", mrb_lfs_dir_free,
};

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

static mrb_value
mrb_s_initialize(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);

  int err = littlefs_ensure_mounted();
  mrb_raise_iff_lfs_error(mrb, err, "lfs_mount");

  struct lfs_info info;
  err = lfs_stat(littlefs_get_lfs(), path, &info);
  if (err == LFS_ERR_OK && info.type != LFS_TYPE_DIR) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Not a directory @ dir_initialize");
  }

  lfs_dir_t *dp = (lfs_dir_t *)mrb_malloc(mrb, sizeof(lfs_dir_t));
  DATA_PTR(self) = dp;
  DATA_TYPE(self) = &mrb_lfs_dir_type;
  err = lfs_dir_open(littlefs_get_lfs(), dp, path);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_dir_open");
  return self;
}

static mrb_value
mrb_Dir_close(mrb_state *mrb, mrb_value self)
{
  lfs_dir_t *dp = (lfs_dir_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_dir_type);
  int err = lfs_dir_close(littlefs_get_lfs(), dp);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_dir_close");
  return mrb_nil_value();
}

static mrb_value
mrb_findnext(mrb_state *mrb, mrb_value self)
{
  lfs_dir_t *dp = (lfs_dir_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_dir_type);
  mrb_value pat_val = mrb_iv_get(mrb, self, MRB_IVSYM(pat));
  const char *pat = mrb_nil_p(pat_val) ? "*" : mrb_str_to_cstr(mrb, pat_val);

  struct lfs_info info;
  int res;
  while ((res = lfs_dir_read(littlefs_get_lfs(), dp, &info)) > 0) {
    /* Skip . and .. */
    if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
      continue;
    }
    if (simple_match(pat, info.name)) {
      return mrb_str_new_cstr(mrb, info.name);
    }
  }
  return mrb_nil_value();
}

static mrb_value
mrb_pat_e(mrb_state *mrb, mrb_value self)
{
  mrb_value pat;
  mrb_get_args(mrb, "S", &pat);
  mrb_iv_set(mrb, self, MRB_IVSYM(pat), pat);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_read(mrb_state *mrb, mrb_value self)
{
  lfs_dir_t *dp = (lfs_dir_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_dir_type);
  struct lfs_info info;
  int res;
  while ((res = lfs_dir_read(littlefs_get_lfs(), dp, &info)) > 0) {
    /* Skip . and .. */
    if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) {
      continue;
    }
    return mrb_str_new_cstr(mrb, info.name);
  }
  return mrb_nil_value();
}

static mrb_value
mrb_rewind(mrb_state *mrb, mrb_value self)
{
  lfs_dir_t *dp = (lfs_dir_t *)mrb_data_get_ptr(mrb, self, &mrb_lfs_dir_type);
  int err = lfs_dir_rewind(littlefs_get_lfs(), dp);
  mrb_raise_iff_lfs_error(mrb, err, "lfs_dir_rewind");
  return self;
}

void
mrb_init_class_Littlefs_Dir(mrb_state *mrb, struct RClass *class_LFS)
{
  struct RClass *class_LFS_Dir = mrb_define_class_under_id(
    mrb, class_LFS, MRB_SYM(Dir), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_LFS_Dir, MRB_TT_CDATA);

  mrb_define_method_id(mrb, class_LFS_Dir, MRB_SYM(initialize), mrb_s_initialize, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS_Dir, MRB_SYM(close), mrb_Dir_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_Dir, MRB_SYM_E(pat), mrb_pat_e, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_LFS_Dir, MRB_SYM(findnext), mrb_findnext, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_Dir, MRB_SYM(read), mrb_read, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_LFS_Dir, MRB_SYM(rewind), mrb_rewind, MRB_ARGS_NONE());
}
