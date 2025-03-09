static void
c_unixtime_offset_eq(struct VM *vm, mrbc_value v[], int argc)
{
  unixtime_offset = (time_t)GET_INT_ARG(1);
  SET_INT_RETURN((mrbc_int_t)unixtime_offset);
}

/*
 * Usage: FAT._erase(num)
 * params
 *   - num: Drive numnber in Integer
 */
static void
c__erase(struct VM *vm, mrbc_value v[], int argc)
{
  int i;
  char *volume = (char *)GET_STRING_ARG(1);
  volume[strlen(volume) - 1] = '\0'; /* remove ":" */
  for (i = 0; i < FF_VOLUMES; i++) {
    if (strcmp(VolumeStr[i], (const char *)volume) == 0) break;
  }
  if (i < FF_VOLUMES) {
    disk_erase(i);
    SET_INT_RETURN(0);
  } else {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Volume not found in disk_erase");
  }
}

static void
c__mkfs(struct VM *vm, mrbc_value v[], int argc)
{
  void *work = mrbc_alloc(vm, FF_MAX_SS);
  const MKFS_PARM opt = {
    FM_FAT,  // fmt
    1,       // n_fat: number of FAT copies
    0,       // n_align: BLOCK_SIZE (== DISK_ERASE_UNIT_SIZE / SECTOR_SIZE) from ioctl
    0,       // n_root: number of root directory entries
    0        // au_size
  };
  FRESULT res;
  res = f_mkfs((const TCHAR *)GET_STRING_ARG(1), &opt, work, FF_MAX_SS);
  mrbc_raise_iff_f_error(vm, res, "f_mkfs");
  mrbc_free(vm, work);
  SET_INT_RETURN(0);
}

static void
c_getfree(struct VM *vm, mrbc_value v[], int argc)
{
  FATFS *fs = (FATFS *)v->instance->data;
  DWORD fre_clust, fre_sect, tot_sect;
  FRESULT res = f_getfree((const TCHAR *)GET_STRING_ARG(1), &fre_clust, &fs);
  mrbc_raise_iff_f_error(vm, res, "f_getfree");
  tot_sect = (fs->n_fatent - 2) * fs->csize;
  fre_sect = fre_clust * fs->csize;
  SET_INT_RETURN((tot_sect << 16) | fre_sect);
}

static void
c__mount(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value fatfs = mrbc_instance_new(vm, v->cls, sizeof(FATFS));
  FATFS *fs = (FATFS *)fatfs.instance->data;
  FRESULT res;
  res = f_mount(fs, (const TCHAR *)GET_STRING_ARG(1), 1);
  mrbc_raise_iff_f_error(vm, res, "f_mount");
  SET_INT_RETURN(0);
}

static void
c__unmount(struct VM *vm, mrbc_value v[], int argc)
{
  FRESULT res;
  res = f_mount(0, (const TCHAR *)GET_STRING_ARG(1), 0);
  mrbc_raise_iff_f_error(vm, res, "f_unmount");
  SET_INT_RETURN(0);
}

static void
c__chdir(struct VM *vm, mrbc_value v[], int argc)
{
  FRESULT res;
  res = f_chdir((const TCHAR *)GET_STRING_ARG(1));
  mrbc_raise_iff_f_error(vm, res, "f_chdir");
  SET_INT_RETURN(0);
}

static void
c__utime(struct VM *vm, mrbc_value v[], int argc)
{
  FILINFO fno = {0};
  const time_t unixtime = GET_INT_ARG(1);
  unixtime2fno(&unixtime, &fno);
  FRESULT res = f_utime((const TCHAR *)GET_STRING_ARG(2), &fno);
  mrbc_raise_iff_f_error(vm, res, "f_utime");
  SET_INT_RETURN(1);
}

static void
c__mkdir(struct VM *vm, mrbc_value v[], int argc)
{
  FRESULT res = f_mkdir((const TCHAR *)GET_STRING_ARG(1));
  mrbc_raise_iff_f_error(vm, res, "f_mkdir");
  SET_INT_RETURN(0);
}

static void
c__chmod(mrbc_vm *vm, mrbc_value v[], int argc)
{
  BYTE attr = GET_INT_ARG(1);
  const TCHAR *path = (const TCHAR *)GET_STRING_ARG(2);
  FRESULT res = f_chmod(path, attr, AM_RDO|AM_ARC|AM_SYS|AM_HID);
  mrbc_raise_iff_f_error(vm, res, "f_chmod");
  SET_INT_RETURN(0);
}

static void
c__stat(mrbc_vm *vm, mrbc_value v[], int argc)
{
  TCHAR *path = (TCHAR *)GET_STRING_ARG(1);
  FILINFO fno = {0};
  FRESULT res = f_stat(path, &fno);
  mrbc_raise_iff_f_error(vm, res, "f_stat");
  mrbc_value stat = mrbc_hash_new(vm, 3);

  time_t unixtime = fno2unixtime(&fno);

  mrbc_hash_set(
    &stat,
    &mrbc_symbol_value(mrbc_str_to_symid("size")),
    &mrbc_integer_value(fno.fsize)
  );
  mrbc_hash_set(
    &stat,
    &mrbc_symbol_value(mrbc_str_to_symid("unixtime")),
    &mrbc_integer_value((mrbc_int_t)unixtime)
  );
  mrbc_hash_set(
    &stat,
    &mrbc_symbol_value(mrbc_str_to_symid("mode")),
    &mrbc_integer_value(fno.fattrib)
  );
  SET_RETURN(stat);
}

static void
c__directory_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  TCHAR *path = (TCHAR *)GET_STRING_ARG(1);
  FILINFO fno = {0};
  FRESULT res = f_stat(path, &fno);
  if (res == FR_OK && (fno.fattrib & AM_DIR)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c__setlabel(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FRESULT res = f_setlabel((const TCHAR *)GET_STRING_ARG(1));
  mrbc_raise_iff_f_error(vm, res, "f_setlabel");
  SET_INT_RETURN(0);
}

static void
c__getlabel(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const TCHAR *path = (const TCHAR *)GET_STRING_ARG(1);
  // Max label length depends on FF_USE_LFN, FF_FS_EXFAT and FF_LFN_UNICODE
  // see picoruby-filesystem-fat/lib/ff14b/documents/doc/getlabel.html
  TCHAR label[12];
  FRESULT res = f_getlabel(path, label, NULL);
  mrbc_raise_iff_f_error(vm, res, "f_getlabel");
  mrbc_value label_val = mrbc_string_new_cstr(vm, label);
  SET_RETURN(label_val);
}

/*
 * Check if file is contiguous in the FAT sectors
 */
static void
c__contiguous_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  FRESULT res;
  FSIZE_t file_size;
  DWORD prev_sect = 0;
  FSIZE_t offset = 0;
  FIL fil;
  const FSIZE_t sector_size = (const FSIZE_t)FILE_sector_size();
  BYTE mode = FA_READ;
  const TCHAR *path = (const TCHAR *)GET_STRING_ARG(1);

  FILINFO fno = {0};
  res = f_stat(path, &fno);
  if (res == FR_OK && (fno.fattrib & AM_DIR)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Is a directory");
    return;
  }

  res = f_open(&fil, path, mode);
  mrbc_raise_iff_f_error(vm, res, "f_open in File.contiguous?");
  if (res != FR_OK) return;

  file_size = f_size(&fil);
  if (file_size < sector_size) {
    SET_TRUE_RETURN();
    goto CLOSE;
  }

  prev_sect = fil.sect;
  if (prev_sect == 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Invalid sector number");
    goto CLOSE;
  }

  while (offset < file_size) {
    offset += sector_size;
    res = f_lseek(&fil, offset);
    mrbc_raise_iff_f_error(vm, res, "f_lseek in File.contiguous?");
    if (prev_sect + 1 != fil.sect) {
      SET_FALSE_RETURN();
      goto CLOSE;
    }
    prev_sect = fil.sect;
  }

  SET_TRUE_RETURN();
CLOSE:
  f_close(&fil);
}


void
mrbc_raise_iff_f_error(mrbc_vm *vm, FRESULT res, const char *func)
{
  char buff[64];
  if (FAT_prepare_exception(res, buff, func) < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), buff);
  }
}


#ifdef USE_FAT_SD_DISK
void
c_FAT_init_spi(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *unit_name = (const char *)GET_STRING_ARG(1);
  if (FAT_set_spi_unit(unit_name, GET_INT_ARG(2), GET_INT_ARG(3), GET_INT_ARG(4), GET_INT_ARG(5)) < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Invalid SPI unit.");
    return;
  }
  SET_INT_RETURN(0);
}
#endif


void
mrbc_filesystem_fat_init(mrbc_vm *vm)
{
  mrbc_class *class_FAT = mrbc_define_class(vm, "FAT", mrbc_class_object);
  mrbc_define_method(vm, class_FAT, "unixtime_offset=", c_unixtime_offset_eq);
  mrbc_define_method(vm, class_FAT, "_erase", c__erase);
  mrbc_define_method(vm, class_FAT, "_mkfs", c__mkfs);
  mrbc_define_method(vm, class_FAT, "getfree", c_getfree);
  mrbc_define_method(vm, class_FAT, "_mount", c__mount);
  mrbc_define_method(vm, class_FAT, "_unmount", c__unmount);
  mrbc_define_method(vm, class_FAT, "_chdir", c__chdir);
  mrbc_define_method(vm, class_FAT, "_utime", c__utime);
  mrbc_define_method(vm, class_FAT, "_mkdir", c__mkdir);
  mrbc_define_method(vm, class_FAT, "_unlink", c__unlink);
  mrbc_define_method(vm, class_FAT, "_rename", c__rename);
  mrbc_define_method(vm, class_FAT, "_chmod", c__chmod);
  mrbc_define_method(vm, class_FAT, "_exist?", c__exist_q);
  mrbc_define_method(vm, class_FAT, "_directory?", c__directory_q);
  mrbc_define_method(vm, class_FAT, "_setlabel", c__setlabel);
  mrbc_define_method(vm, class_FAT, "_getlabel", c__getlabel);
  mrbc_define_method(vm, class_FAT, "_contiguous?", c__contiguous_q);
  mrbc_init_class_FAT_Dir(vm, class_FAT);
  mrbc_init_class_FAT_File(vm, class_FAT);

  mrbc_class *class_FAT_Stat = mrbc_define_class_under(vm, class_FAT, "Stat", mrbc_class_object);
  mrbc_define_method(vm, class_FAT_Stat, "_stat", c__stat);

#ifdef USE_FAT_SD_DISK
  mrbc_define_method(vm, class_FAT, "init_spi", c_FAT_init_spi);
#endif
}


void
c__exist_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  TCHAR *path = (TCHAR *)GET_STRING_ARG(1);
  FILINFO fno = {0};
  FRESULT res = f_stat(path, &fno);
  if (res == FR_OK) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
c__unlink(mrbc_vm *vm, mrbc_value v[], int argc)
{
  TCHAR *path = (TCHAR *)GET_STRING_ARG(1);
  FRESULT res = f_unlink(path);
  mrbc_raise_iff_f_error(vm, res, "f_unlink");
  SET_INT_RETURN(0);
}

void
c__rename(mrbc_vm *vm, mrbc_value v[], int argc)
{
  TCHAR *from = (TCHAR *)GET_STRING_ARG(1);
  TCHAR *to = (TCHAR *)GET_STRING_ARG(2);
  FRESULT res = f_rename(from, to);
  mrbc_raise_iff_f_error(vm, res, "f_rename");
  SET_INT_RETURN(0);
}
