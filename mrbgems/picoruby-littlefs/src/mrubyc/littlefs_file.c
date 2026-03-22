static mrbc_class *class_Littlefs_File;
static mrbc_class *class_Littlefs_VFSMethods;

static void
c_new(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *path = (const char *)GET_STRING_ARG(1);
  const char *mode_str = (const char *)GET_STRING_ARG(2);

  int err = littlefs_ensure_mounted();
  mrbc_raise_iff_lfs_error(vm, err, "lfs_mount");

  mrbc_value _file = mrbc_instance_new(vm, v->cls, sizeof(lfs_file_data_t));
  lfs_file_data_t *fd = (lfs_file_data_t *)_file.instance->data;

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
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Unknown file open mode");
    return;
  }

  strncpy(fd->path, path, LFS_NAME_MAX);
  fd->path[LFS_NAME_MAX] = '\0';

  err = lfs_file_open(littlefs_get_lfs(), &fd->file, path, flags);
  mrbc_raise_iff_lfs_error(vm, err, path);
  fd->is_open = true;
  _file.instance->cls = class_Littlefs_File;
  SET_RETURN(_file);
}

static void
c_sector_size(mrbc_vm *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(littlefs_get_config()->block_size);
}

static void
c_tell(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  lfs_soff_t pos = lfs_file_tell(littlefs_get_lfs(), &fd->file);
  if (pos < 0) {
    mrbc_raise_iff_lfs_error(vm, (int)pos, "lfs_file_tell");
    return;
  }
  SET_INT_RETURN(pos);
}

static void
c_seek(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  int ofs = GET_INT_ARG(1);
  int whence = GET_INT_ARG(2);

  int lfs_whence;
  if (whence == SEEK_SET) {
    lfs_whence = LFS_SEEK_SET;
  } else if (whence == SEEK_CUR) {
    lfs_whence = LFS_SEEK_CUR;
  } else if (whence == SEEK_END) {
    lfs_whence = LFS_SEEK_END;
  } else {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Unknown whence");
    return;
  }

  lfs_soff_t new_pos = lfs_file_seek(littlefs_get_lfs(), &fd->file, (lfs_soff_t)ofs, lfs_whence);
  if (new_pos < 0) {
    mrbc_raise_iff_lfs_error(vm, (int)new_pos, "lfs_file_seek");
    return;
  }
  SET_INT_RETURN(0);
}

static void
c_size(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  lfs_soff_t sz = lfs_file_size(littlefs_get_lfs(), &fd->file);
  if (sz < 0) {
    mrbc_raise_iff_lfs_error(vm, (int)sz, "lfs_file_size");
    return;
  }
  SET_INT_RETURN(sz);
}

static void
c_eof_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  lfs_soff_t pos = lfs_file_tell(littlefs_get_lfs(), &fd->file);
  lfs_soff_t sz = lfs_file_size(littlefs_get_lfs(), &fd->file);
  if (pos < 0 || sz < 0 || sz <= pos) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_read(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  lfs_size_t btr = GET_INT_ARG(1);
  char buff[btr];
  lfs_ssize_t br = lfs_file_read(littlefs_get_lfs(), &fd->file, buff, btr);
  if (br < 0) {
    mrbc_raise_iff_lfs_error(vm, (int)br, "lfs_file_read");
    return;
  }
  if (0 < br) {
    mrbc_value value = mrbc_string_new(vm, (const void *)buff, br);
    SET_RETURN(value);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_getbyte(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  char c;
  lfs_ssize_t br = lfs_file_read(littlefs_get_lfs(), &fd->file, &c, 1);
  if (br == 1) {
    SET_INT_RETURN((unsigned char)c);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_write(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  mrbc_value str = v[1];
  lfs_ssize_t bw = lfs_file_write(
    littlefs_get_lfs(), &fd->file, str.string->data, str.string->size);
  if (bw < 0) {
    mrbc_raise_iff_lfs_error(vm, (int)bw, "lfs_file_write");
    return;
  }
  int err = lfs_file_sync(littlefs_get_lfs(), &fd->file);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_file_sync");
  SET_INT_RETURN(bw);
}

static void
c_close(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  bool writable = fd->writable;
  char path[LFS_NAME_MAX + 1];
  strncpy(path, fd->path, LFS_NAME_MAX);
  path[LFS_NAME_MAX] = '\0';
  fd->is_open = false;
  int err = lfs_file_close(littlefs_get_lfs(), &fd->file);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_file_close");
  if (writable) {
    uint32_t ts = littlefs_get_unixtime();
    lfs_setattr(littlefs_get_lfs(), path, LFS_ATTR_MTIME, &ts, sizeof(ts));
  }
  SET_NIL_RETURN();
}

static void
file_destructor(mrbc_value *v)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  if (fd->is_open) {
    lfs_file_close(littlefs_get_lfs(), &fd->file);
  }
}

static void
c_expand(mrbc_vm *vm, mrbc_value v[], int argc)
{
  /* no-op */
  mrbc_int_t size = GET_INT_ARG(1);
  SET_INT_RETURN(size);
}

static void
c_fsync(mrbc_vm *vm, mrbc_value v[], int argc)
{
  lfs_file_data_t *fd = (lfs_file_data_t *)v->instance->data;
  int err = lfs_file_sync(littlefs_get_lfs(), &fd->file);
  mrbc_raise_iff_lfs_error(vm, err, "lfs_file_sync");
  SET_INT_RETURN(0);
}

static void
c_vfs_methods(mrbc_vm *vm, mrbc_value v[], int argc)
{
  prb_vfs_methods m = {
    c_new,
    c_close,
    c_read,
    c_getbyte,
    c_write,
    c_seek,
    c_tell,
    c_size,
    c_fsync,
    c__exist_q,
    c__unlink
  };
  mrbc_value methods = mrbc_instance_new(vm, class_Littlefs_VFSMethods, sizeof(prb_vfs_methods));
  memcpy(methods.instance->data, &m, sizeof(prb_vfs_methods));
  SET_RETURN(methods);
}

void
mrbc_init_class_Littlefs_File(mrbc_vm *vm, mrbc_class *class_LFS)
{
  class_Littlefs_File = mrbc_define_class_under(vm, class_LFS, "File", mrbc_class_object);
  mrbc_define_destructor(class_Littlefs_File, file_destructor);
  class_Littlefs_VFSMethods = mrbc_define_class_under(vm, class_LFS, "VFSMethods", mrbc_class_object);

  mrbc_define_method(vm, class_Littlefs_File, "new", c_new);
  mrbc_define_method(vm, class_Littlefs_File, "open", c_new);
  mrbc_define_method(vm, class_Littlefs_File, "tell", c_tell);
  mrbc_define_method(vm, class_Littlefs_File, "seek", c_seek);
  mrbc_define_method(vm, class_Littlefs_File, "eof?", c_eof_q);
  mrbc_define_method(vm, class_Littlefs_File, "read", c_read);
  mrbc_define_method(vm, class_Littlefs_File, "getbyte", c_getbyte);
  mrbc_define_method(vm, class_Littlefs_File, "write", c_write);
  mrbc_define_method(vm, class_Littlefs_File, "close", c_close);
  mrbc_define_method(vm, class_Littlefs_File, "size", c_size);
  mrbc_define_method(vm, class_Littlefs_File, "expand", c_expand);
  mrbc_define_method(vm, class_Littlefs_File, "fsync", c_fsync);
  mrbc_define_method(vm, class_Littlefs_File, "sector_size", c_sector_size);

  mrbc_define_method(vm, class_LFS, "vfs_methods", c_vfs_methods);
}
