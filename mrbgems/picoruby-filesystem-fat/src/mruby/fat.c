#include <string.h>
#include "mruby/data.h"
#include "mruby/class.h"
#include "mruby/hash.h"
#include "mruby/string.h"
#include "mruby/presym.h"
#include "mruby/variable.h"

typedef struct {
  FATFS fs;
  char *prefix;
} fatfs_t;

static void
mrb_fatfs_free(mrb_state *mrb, void *ptr) {
  FRESULT res;
  fatfs_t *mrb_fs = (fatfs_t *)ptr;
  if (mrb_fs) {
    if (mrb_fs->prefix) {
      res = f_mount(0, (const TCHAR *)mrb_fs->prefix, 0);
      mrb_raise_iff_f_error(mrb, res, "f_mount");
    }
    mrb_free(mrb, mrb_fs);
  }
}

struct mrb_data_type mrb_fatfs_type = {
  "FATFS", mrb_fatfs_free
};


void
mrb_raise_iff_f_error(mrb_state *mrb, FRESULT res, const char *func)
{
  char buff[64];
  if (FAT_prepare_exception(res, buff, func) < 0) {
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

/*
 * Usage: FAT._erase(num)
 * params
 *   - num: Drive numnber in Integer
 */
static mrb_value
mrb__erase(mrb_state *mrb, mrb_value self)
{
  int i;
  char *volume;
  mrb_get_args(mrb, "z", &volume);
  volume[strlen(volume) - 1] = '\0'; /* remove ":" */
  for (i = 0; i < FF_VOLUMES; i++) {
    if (strcmp(VolumeStr[i], (const char *)volume) == 0) break;
  }
  if (i < FF_VOLUMES) {
    disk_erase(i);
  } else {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Volume not found in disk_erase");
  }
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__mkfs(mrb_state *mrb, mrb_value self)
{
  void *work = mrb_malloc(mrb, FF_MAX_SS);
  const MKFS_PARM opt = {
    FM_FAT,  // fmt
    1,       // n_fat: number of FAT copies
    0,       // n_align: BLOCK_SIZE (== DISK_ERASE_UNIT_SIZE / SECTOR_SIZE) from ioctl
    0,       // n_root: number of root directory entries
    0        // au_size
  };
  FRESULT res;
  const char *path;
  mrb_get_args(mrb, "z", &path);
  res = f_mkfs((const TCHAR *)path, &opt, work, FF_MAX_SS);
  mrb_raise_iff_f_error(mrb, res, "f_mkfs");
  mrb_free(mrb, work);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_getfree(mrb_state *mrb, mrb_value self)
{
  DWORD fre_clust, fre_sect, tot_sect;
  const char *path;
  mrb_get_args(mrb, "z", &path);
  fatfs_t *mrb_fs = (fatfs_t *)mrb_data_get_ptr(mrb, self, &mrb_fatfs_type);
  FATFS *fs = &mrb_fs->fs;
  FRESULT res = f_getfree((const TCHAR *)path, &fre_clust, &fs);
  mrb_raise_iff_f_error(mrb, res, "f_getfree");
  tot_sect = (fs->n_fatent - 2) * fs->csize;
  fre_sect = fre_clust * fs->csize;
  return mrb_fixnum_value((tot_sect << 16) | fre_sect);
}

static mrb_value
mrb__mount(mrb_state *mrb, mrb_value self)
{
  fatfs_t *mrb_fs = (fatfs_t *)mrb_malloc(mrb, sizeof(fatfs_t));
  DATA_PTR(self) = mrb_fs;
  DATA_TYPE(self) = &mrb_fatfs_type;
  FATFS *fs = &mrb_fs->fs;
  mrb_value prefix = mrb_iv_get(mrb, self, MRB_IVSYM(prefix));
  if (mrb_nil_p(prefix)) {
    mrb_fs->prefix = NULL;
    mrb_raise(mrb, E_RUNTIME_ERROR, "Prefix not found in FATFS#_mount");
  }
  mrb_fs->prefix = RSTRING_PTR(prefix);
  FRESULT res;
  const char *path;
  mrb_get_args(mrb, "z", &path);
  res = f_mount(fs, (const TCHAR *)path, 1);
  mrb_raise_iff_f_error(mrb, res, "f_mount");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__unmount(mrb_state *mrb, mrb_value self)
{
  const char *prefix;
  mrb_get_args(mrb, "z", &prefix);
  (void)prefix;
  fatfs_t *mrb_fs = (fatfs_t *)mrb_data_get_ptr(mrb, self, &mrb_fatfs_type);
  mrb_fatfs_free(mrb, mrb_fs);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__chdir(mrb_state *mrb, mrb_value self)
{
  FRESULT res;
  const char *name;
  mrb_get_args(mrb, "z", &name);
  res = f_chdir((const TCHAR *)name);
  mrb_raise_iff_f_error(mrb, res, "f_chdir");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__utime(mrb_state *mrb, mrb_value self)
{
  FILINFO fno = {0};
  mrb_int unixtime;
  const char *name;
  mrb_get_args(mrb, "iz", &unixtime, &name);
  unixtime2fno((const time_t *)&unixtime, &fno);
  FRESULT res = f_utime((const TCHAR *)name, &fno);
  mrb_raise_iff_f_error(mrb, res, "f_utime");
  return mrb_fixnum_value(1);
}

static mrb_value
mrb__mkdir(mrb_state *mrb, mrb_value self)
{
  const char *name;
  mrb_int mode = 0;
  mrb_get_args(mrb, "z|i", &name, &mode);
  (void)mode;
  FRESULT res = f_mkdir((const TCHAR *)name);
  mrb_raise_iff_f_error(mrb, res, "f_mkdir");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__chmod(mrb_state *mrb, mrb_value self)
{
  mrb_int attr;
  const char *path;
  mrb_get_args(mrb, "iz", &attr, &path);
  FRESULT res = f_chmod((const TCHAR *)path, (BYTE)attr, AM_RDO|AM_ARC|AM_SYS|AM_HID);
  mrb_raise_iff_f_error(mrb, res, "f_chmod");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__stat(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  FILINFO fno = {0};
  FRESULT res = f_stat((TCHAR *)path, &fno);
  mrb_raise_iff_f_error(mrb, res, "f_stat");
  mrb_value stat = mrb_hash_new_capa(mrb, 3);

  time_t unixtime = fno2unixtime(&fno);

  mrb_hash_set(mrb,
    stat,
    mrb_symbol_value(MRB_SYM(size)),
    mrb_fixnum_value(fno.fsize)
  );
  mrb_hash_set(mrb,
    stat,
    mrb_symbol_value(MRB_SYM(unixtime)),
    mrb_fixnum_value((mrb_int)unixtime)
  );
  mrb_hash_set(mrb,
    stat,
    mrb_symbol_value(MRB_SYM(mode)),
    mrb_fixnum_value(fno.fattrib)
  );
  return stat;
}

static mrb_value
mrb__directory_p(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  FILINFO fno = {0};
  FRESULT res = f_stat((TCHAR *)path, &fno);
  if (res == FR_OK && (fno.fattrib & AM_DIR)) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb__setlabel(mrb_state *mrb, mrb_value self)
{
  const char *lable;
  mrb_get_args(mrb, "z", &lable);
  FRESULT res = f_setlabel((const TCHAR *)lable);
  mrb_raise_iff_f_error(mrb, res, "f_setlabel");
  return mrb_fixnum_value(0);
}

static mrb_value
mrb__getlabel(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  // Max label length depends on FF_USE_LFN, FF_FS_EXFAT and FF_LFN_UNICODE
  // see picoruby-filesystem-fat/lib/ff14b/documents/doc/getlabel.html
  TCHAR label[12];
  FRESULT res = f_getlabel((const TCHAR *)path, label, NULL);
  mrb_raise_iff_f_error(mrb, res, "f_getlabel");
  return mrb_str_new_cstr(mrb, label);
}

/*
 * Check if file is contiguous in the FAT sectors
 */
static mrb_value
mrb__contiguous_p(mrb_state *mrb, mrb_value self)
{
  FRESULT res;
  FSIZE_t file_size;
  DWORD prev_sect = 0;
  FSIZE_t offset = 0;
  FIL fil;
  const FSIZE_t sector_size = (const FSIZE_t)FILE_sector_size();
  BYTE mode = FA_READ;
  const char *path;
  mrb_get_args(mrb, "z", &path);

  FILINFO fno = {0};
  res = f_stat((const TCHAR *)path, &fno);
  if (res == FR_OK && (fno.fattrib & AM_DIR)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Is a directory");
  }

  res = f_open(&fil, (const TCHAR *)path, mode);
  mrb_raise_iff_f_error(mrb, res, "f_open in File.contiguous?");

  file_size = f_size(&fil);
  if (file_size < sector_size) {
    f_close(&fil);
    return mrb_true_value();
  }

  prev_sect = fil.sect;
  if (prev_sect == 0) {
    f_close(&fil);
    mrb_raise(mrb, E_RUNTIME_ERROR, "Invalid sector number");
  }

  while (offset < file_size) {
    offset += sector_size;
    res = f_lseek(&fil, offset);
    mrb_raise_iff_f_error(mrb, res, "f_lseek in File.contiguous?");
    if (prev_sect + 1 != fil.sect) {
      f_close(&fil);
      return mrb_false_value();
    }
    prev_sect = fil.sect;
  }

  return mrb_true_value();
}



#ifdef USE_FAT_SD_DISK
void
mrb_FAT_init_spi(mrb_state *mrb, mrb_value self)
{
  const char *unit_name;
  mrb_int sck, cipo, copi, cs;
  mrb_get_args(mrb, "ziiii", &unit_name, &sck, &cipo, &copi, &cs);
  if (FAT_set_spi_unit(unit_name, sck, cipo, copi, cs) < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Invalid SPI unit.");
    return;
  }
  return mrb_fixnum_value(0);
}
#endif


void
mrb_picoruby_filesystem_fat_gem_init(mrb_state* mrb)
{
  struct RClass *class_FAT = mrb_define_class_id(mrb, MRB_SYM(FAT), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_FAT, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_FAT, MRB_SYM_E(unixtime_offset), mrb_unixtime_offset_e, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_erase), mrb__erase, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_mkfs), mrb__mkfs, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(getfree), mrb_getfree, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_mount), mrb__mount, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_unmount), mrb__unmount, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_chdir), mrb__chdir, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_utime), mrb__utime, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_mkdir), mrb__mkdir, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_unlink), mrb__unlink, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_rename), mrb__rename, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_chmod), mrb__chmod, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM_Q(_exist), mrb__exist_p, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM_Q(_directory), mrb__directory_p, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_setlabel), mrb__setlabel, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM(_getlabel), mrb__getlabel, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_FAT, MRB_SYM_Q(_contiguous), mrb__contiguous_p, MRB_ARGS_REQ(1));
  mrb_init_class_FAT_Dir(mrb, class_FAT);
  mrb_init_class_FAT_File(mrb, class_FAT);

  struct RClass *class_FAT_Stat = mrb_define_class_under_id(mrb, class_FAT, MRB_SYM(Stat), mrb->object_class);
  mrb_define_method_id(mrb, class_FAT_Stat, MRB_SYM(_stat), mrb__stat, MRB_ARGS_REQ(1));

#ifdef USE_FAT_SD_DISK
  mrb_define_method(mrb, class_FAT, MRB_SYM(init_spi), mrb_FAT_init_spi, MRB_ARGS_REQ(5));
#endif
}

void
mrb_picoruby_filesystem_fat_gem_final(mrb_state* mrb)
{
}

mrb_value
mrb__exist_p(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  FILINFO fno = {0};
  FRESULT res = f_stat((TCHAR *)path, &fno);
  if (res == FR_OK) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

mrb_value
mrb__unlink(mrb_state *mrb, mrb_value self)
{
  const char *path;
  mrb_get_args(mrb, "z", &path);
  FRESULT res = f_unlink((TCHAR *)path);
  mrb_raise_iff_f_error(mrb, res, "f_unlink");
  return mrb_fixnum_value(0);
}

mrb_value
mrb__rename(mrb_state *mrb, mrb_value self)
{
  const char *from, *to;
  mrb_get_args(mrb, "zz", &from, &to);
  FRESULT res = f_rename((TCHAR *)from, (TCHAR *)to);
  mrb_raise_iff_f_error(mrb, res, "f_rename");
  return mrb_fixnum_value(0);
}
