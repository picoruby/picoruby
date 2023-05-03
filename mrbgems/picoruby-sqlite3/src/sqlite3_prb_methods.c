#include "../include/sqlite3_prb_methods.h"

#include <stdio.h>

#define FLASH_VOLUME_NAME "flash"
#define SEEK_SET 0

//#define D() printf("debug: %s\n", __func__)
#define D() (void)0

void
vfs_funcall(
  void (*func)(mrbc_vm *, mrbc_value *, int),
  mrbc_value *v,
  int argc
)
{
  /*
   * TODO: when mrbc_decref() should be called?
   */
  mrbc_incref(&v[0]);
  func(prbvfs.pAppData, &v[0], argc);
}

mrbc_int_t
prb_time_gettime_us(void)
{
  D();
  mrbc_value v[1];
  v[0] = mrbc_nil_value();
  vfs_funcall(time_methods.time_now, &v[0], 0);
  if (v->tt == MRBC_TT_NIL) {
    return 0;
  }
  PICORUBY_TIME * data = (PICORUBY_TIME *)v[0].instance->data;
  return data->unixtime_us / 1000;
}

int
prb_file_new(PRBFile *prbfile, const char *zName, int flags)
{
  D();
  mrbc_vm *vm = (mrbc_vm *)prbfile->vm;
  mrbc_value v[3];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  char mode[3];
  if (flags & SQLITE_OPEN_CREATE) {
    if (prb_file_exist_q(&prbvfs, zName)) {
      mode[0] = 'r';
    } else {
      mode[0] = 'w';
    }
    mode[1] = '+';
    mode[2] = '\0';
  } else {
    mode[0] = 'r';
    mode[1] = '\0';
  }
  v[2] = mrbc_string_new_cstr(vm, mode);
  vfs_funcall(vfs_methods.file_new, &v[0], 2);
  if (v->tt == MRBC_TT_NIL) {
    return -1;
  }
  prbfile->file = mrbc_alloc(vm, sizeof(mrbc_value));
  memcpy(prbfile->file, &v[0], sizeof(mrbc_value));

  /* FIXME: retrieve from the real filesystem */
  if (strncmp(zName, FLASH_VOLUME_NAME, sizeof(FLASH_VOLUME_NAME)) == 0) {
    prbfile->sector_size = 4096;
  } else {
    prbfile->sector_size = 512;
  }

  return 0;
}

int prb_file_close(PRBFile *prbfile)
{
  D();
  mrbc_value v[1];
  v[0] = *prbfile->file;
  vfs_funcall(vfs_methods.file_close, &v[0], 0);
  mrbc_raw_free(prbfile->file);
  return 0;
}

int prb_file_read(PRBFile *prbfile, void *zBuf, size_t nBuf)
{
  D();
  mrbc_value v[2];
  v[0] = *prbfile->file;
  v[1] = mrbc_integer_value(nBuf);
  vfs_funcall(vfs_methods.file_read, &v[0], 1);
  if (v->tt == MRBC_TT_NIL) {
    return 0; /* EOF */
  } else if (v->tt != MRBC_TT_STRING) {
    return -1;
  }
  memcpy(zBuf, v[0].string->data, v[0].string->size);
  return v[0].string->size;
}

int prb_file_write(PRBFile *prbfile, const void *zBuf, size_t nBuf)
{
  D();
  mrbc_vm *vm = (mrbc_vm *)prbfile->vm;
  mrbc_value v[3];
  v[0] = *prbfile->file;
  v[1] = mrbc_string_new(vm, zBuf, nBuf);
  v[2] = mrbc_integer_value(nBuf);
  vfs_funcall(vfs_methods.file_write, &v[0], 2);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return v->i;
}

int prb_file_fsync(PRBFile *prbfile)
{
  D();
  mrbc_value v[1];
  v[0] = *prbfile->file;
  vfs_funcall(vfs_methods.file_fsync, &v[0], 0);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return 0;
}

int prb_file_seek(PRBFile *prbfile, int offset)
{
  D();
  mrbc_value v[3];
  v[0] = *prbfile->file;
  v[1] = mrbc_integer_value(offset);
  v[2] = mrbc_integer_value(SEEK_SET); // whence is always SEEK_SET
  vfs_funcall(vfs_methods.file_seek, &v[0], 2);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return 0;
}

int prb_file_tell(PRBFile *prbfile)
{
  D();
  mrbc_value v[1];
  v[0] = *prbfile->file;
  vfs_funcall(vfs_methods.file_tell, &v[0], 0);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return v->i;
}

int prb_file_unlink(sqlite3_vfs *pVfs, const char *zName)
{
  D();
  mrbc_vm *vm = prbvfs.pAppData;
  mrbc_value v[2];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  vfs_funcall(vfs_methods.file_unlink, &v[0], 1);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return 0;
}

int prb_file_exist_q(sqlite3_vfs *pVfs, const char *zName)
{
  D();
  mrbc_vm *vm = prbvfs.pAppData;
  mrbc_value v[2];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  vfs_funcall(vfs_methods.file_exist_q, &v[0], 1);
  return (v->tt == MRBC_TT_TRUE);
}

int prb_file_stat(sqlite3_vfs *pVfs, const char *zName, int stat)
{
  D();
  mrbc_vm *vm = prbvfs.pAppData;
  mrbc_value v[2];
  v[0] = mrbc_nil_value();
  v[1] = mrbc_string_new_cstr(vm, zName);
  vfs_funcall(vfs_methods.file_stat, &v[0], 1);
  if (v->tt != MRBC_TT_INTEGER) {
    return -1;
  }
  return 0; // TODO
}

int
prb_mem_msize(void *p)
{
  D();
#ifdef MRBC_ALLOC_LIBC
  return malloc_usable_size(p);
#else
  return mrbc_alloc_usable_size(p);
#endif
}

int
prb_mem_roundup(int n)
{
  D();
  return n;
}

int
prb_mem_init(void *ignore)
{
  D();
  return SQLITE_OK;
}

void
prb_mem_shutdown(void *ignore)
{
  D();
}

void *
prb_raw_alloc(int nByte)
{
  void *ptr = mrbc_raw_alloc(nByte);
  return ptr;
}

void
prb_raw_free(void *pPrior)
{
  mrbc_raw_free(pPrior);
}

