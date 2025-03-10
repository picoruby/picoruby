#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include <mruby/data.h>

static void
mrb_fat_dir_free(mrb_state *mrb, void *ptr) {
  f_closedir((DIR *)ptr);
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_fat_dir_type = {
  "FATDir", mrb_fat_dir_free,
};


static mrb_value
mrb_s_new(mrb_state *mrb, mrb_value klass)
{
  FRESULT res;
  FILINFO fno = {0};
  const char *path;
  mrb_get_args(mrb, "z", &path);

  res = f_stat((const TCHAR *)path, &fno);
  if (res != FR_INVALID_NAME) {
    /* FIXME: pathname "0:" becomes INVALID, why? */
    mrb_raise_iff_f_error(mrb, res, "f_stat");
  }
  if (res == FR_OK && (fno.fattrib & AM_DIR) == 0) {
    mrb_raise(
      mrb, E_RUNTIME_ERROR, // Errno::ENOTDIR in CRuby
      "Not a directory @ dir_initialize"
    );
  }

  DIR *dp = (DIR *)mrb_malloc(mrb, sizeof(DIR));
  mrb_value dir = mrb_obj_value(Data_Wrap_Struct(mrb, mrb_class(mrb, klass), &mrb_fat_dir_type, dp));
  res = f_opendir(dp, path);
  mrb_raise_iff_f_error(mrb, res, "f_opendir");
  return dir;
}

static mrb_value
mrb_Dir_close(mrb_state *mrb, mrb_value self)
{
  DIR *dp = (DIR *)mrb_data_get_ptr(mrb, self, &mrb_fat_dir_type);
  FRESULT res = f_closedir(dp);
  mrb_raise_iff_f_error(mrb, res, "f_close");
  return mrb_nil_value();
}

static mrb_value
mrb_findnext(mrb_state *mrb, mrb_value self)
{
  DIR *dp = (DIR *)mrb_data_get_ptr(mrb, self, &mrb_fat_dir_type);
  FRESULT fr;
  FILINFO fno = {0};
  fr = f_findnext(dp, &fno);
  if (fr == FR_OK && fno.fname[0]) {
    mrb_value value = mrb_str_new_cstr(mrb, (const char *)(fno.fname));
    return value;
  } else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb_pat_e(mrb_state *mrb, mrb_value self)
{
  DIR *dp = (DIR *)mrb_data_get_ptr(mrb, self, &mrb_fat_dir_type);
  mrb_get_args(mrb, "z", &dp->pat);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_read(mrb_state *mrb, mrb_value self)
{
  DIR *dp = (DIR *)mrb_data_get_ptr(mrb, self, &mrb_fat_dir_type);
  FILINFO fno = {0};
  FRESULT res = f_readdir(dp, &fno);
  mrb_raise_iff_f_error(mrb, res, "f_readdir");
  if (fno.fname[0] == 0) {
    return mrb_nil_value();
  } else {
    mrb_value value = mrb_str_new_cstr(mrb, (const char *)(fno.fname));
    return value;
  }
}

static mrb_value
mrb_rewind(mrb_state *mrb, mrb_value self)
{
  DIR *dp = (DIR *)mrb_data_get_ptr(mrb, self, &mrb_fat_dir_type);
  f_rewinddir(dp);
  return self;
}

void
mrb_init_class_FAT_Dir(mrb_state *mrb, struct RClass *class_FAT)
{
  struct RClass *class_FAT_Dir = mrb_define_class_under_id(mrb, class_FAT, MRB_SYM(Dir), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_FAT_Dir, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_FAT_Dir, MRB_SYM(new), mrb_s_new, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT_Dir, MRB_SYM(close), mrb_Dir_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FAT_Dir, MRB_SYM_E(pat), mrb_pat_e, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT_Dir, MRB_SYM(findnext), mrb_findnext, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FAT_Dir, MRB_SYM(read), mrb_read, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_FAT_Dir, MRB_SYM(rewind), mrb_rewind, MRB_ARGS_NONE());
}
