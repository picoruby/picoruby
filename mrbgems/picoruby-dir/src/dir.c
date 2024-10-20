#include <mrubyc.h>

#include <sys/types.h>
#if defined(_WIN32) || defined(_WIN64)
  #define MAXPATHLEN 1024
 #if !defined(PATH_MAX)
  #define PATH_MAX MAX_PATH
 #endif
  #define S_ISDIR(B) ((B)&_S_IFDIR)
  #include "Win/dirent.c"
  #include <direct.h>
  #define rmdir(path) _rmdir(path)
  #define getcwd(path,len) _getcwd(path,len)
  #define mkdir(path,mode) _mkdir(path)
  #define chdir(path) _chdir(path)
#else
  #include <sys/param.h>
  #include <dirent.h>
  #include <unistd.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

struct picoruby_dir {
  DIR *dir;
};

static void
dir_free(mrbc_vm *vm, void *ptr)
{
  struct picoruby_dir *mdir = (struct picoruby_dir*)ptr;

  if (mdir->dir) {
    closedir(mdir->dir);
    mdir->dir = NULL;
  }
  mrbc_free(vm, mdir);
}

static void
c_dir_close(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct picoruby_dir *mdir;
  mdir = (struct picoruby_dir*)v[0].instance->data;
  if (!mdir) {
    SET_NIL_RETURN();
    return;
  }
  if (!mdir->dir) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "closed directory");
    return;
  }
  if (closedir(mdir->dir) == -1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cannot close directory");
    return;
  }
  mdir->dir = NULL;
  SET_NIL_RETURN();
}

static void
c_dir_init(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DIR *dir;
  struct picoruby_dir *mdir;
  const char *path;

  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(struct picoruby_dir));
  mdir = (struct picoruby_dir *)self.instance->data;
  memset(mdir, 0, sizeof(struct picoruby_dir));

  path = (const char *)GET_STRING_ARG(1);
  if ((dir = opendir(path)) == NULL) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cannot open directory");
    return;
  }
  mdir->dir = dir;
  SET_RETURN(self);
}

static void
c_dir_delete(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *path;

  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
  }
  path = (const char *)GET_STRING_ARG(1);
  if (rmdir(path) == -1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cannot remove directory");
    return;
  }
  SET_INT_RETURN(0);
}

static void
c_dir_exist_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct stat sb;
  const char *path;

  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
  }
  path = (const char *)GET_STRING_ARG(1);
  if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    SET_TRUE_RETURN();
  }
  else {
    SET_FALSE_RETURN();
  }
}

static void
c_dir_getwd(mrbc_vm *vm, mrbc_value v[], int argc)
{
  char *path;
  mrbc_int_t size = 64;

  path = (char *)mrbc_alloc(vm, size);
  while (getcwd(path, size) == NULL) {
    int e = errno;
    if (e != ERANGE) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cannot get current directory");
      return;
    }
    size *= 2;
    mrbc_realloc(vm, path, size);
  }
  SET_RETURN(mrbc_string_new_cstr(vm, path));
  mrbc_free(vm, path);
}

static void
c_dir_mkdir(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t mode;
  const char *path;

  mode = 0777;
  if (argc < 1 || 2 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  path = (const char *)GET_STRING_ARG(1);
  if (argc == 2) {
    mode = GET_INT_ARG(2);
  }
  if (mkdir(path, mode) == -1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cannot create directory");
    return;
  }
  SET_INT_RETURN(0);
}

static void
c_dir__chdir(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *path;

  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  path = (const char *)GET_STRING_ARG(1);
  if (chdir(path) == -1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cannot change directory");
    return;
  }
  SET_INT_RETURN(0);
}

static void
c_dir_chroot(mrbc_vm *vm, mrbc_value v[], int argc)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__ANDROID__) || defined(__MSDOS__)
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "dirtell() unreliable on your system");
  SET_INT_RETURN(0);
  return;
#else
  const char *path;
  int res;

  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  path = (const char *)GET_STRING_ARG(1);
  res = chroot(path);
  if (res == -1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cannot change root directory");
    return;
  }

  SET_INT_RETURN(res);
#endif
}

static bool
skip_name_p(const char *name)
{
  if (name[0] != '.') return false;
  if (name[1] == '\0') return true;
  if (name[1] != '.') return false;
  if (name[2] == '\0') return true;
  return false;
}

static void
c_dir_empty_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DIR *dir;
  struct dirent *dp;
  const char *path;

  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
  }
  path = (const char *)GET_STRING_ARG(1);
  if ((dir = opendir(path)) == NULL) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cannot open directory");
    return;
  }
  while ((dp = readdir(dir))) {
    if (!skip_name_p(dp->d_name)) {
      SET_FALSE_RETURN();
      break;
    }
  }
  closedir(dir);
  SET_TRUE_RETURN();
}

static void
c_dir_read(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct picoruby_dir *mdir;
  struct dirent *dp;

  mdir = (struct picoruby_dir*)v[0].instance->data;
  if (!mdir) {
    SET_NIL_RETURN();
    return;
  }
  if (!mdir->dir) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "closed directory");
    return;
  }
  dp = readdir(mdir->dir);
  if (dp != NULL) {
    mrbc_value str = mrbc_string_new_cstr(vm, dp->d_name);
    SET_RETURN(str);
  }
  else {
    SET_NIL_RETURN();
  }
}

static void
c_dir_rewind(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct picoruby_dir *mdir;

  mdir = (struct picoruby_dir*)v[0].instance->data;
  if (!mdir) {
    SET_NIL_RETURN();
    return;
  }
  if (!mdir->dir) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "closed directory");
    return;
  }
  rewinddir(mdir->dir);
}

static void
c_dir_seek(mrbc_vm *vm, mrbc_value v[], int argc)
{
  #if defined(_WIN32) || defined(_WIN64) || defined(__ANDROID__)
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "dirtell() unreliable on your system");
  return;
  #else
  struct picoruby_dir *mdir;

  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mdir = (struct picoruby_dir*)v[0].instance->data;
  if (!mdir) {
    SET_NIL_RETURN();
    return;
  }
  if (!mdir->dir) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "closed directory");
    return;
  }
  seekdir(mdir->dir, (long)GET_INT_ARG(1));
  #endif
}

static void
c_dir_tell(mrbc_vm *vm, mrbc_value v[], int argc)
{
#if defined(_WIN32) || defined(_WIN64) || defined(__ANDROID__)
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "dirtell() unreliable on your system");
  SET_INT_RETURN(0);
  return;
#else
  struct picoruby_dir *mdir;
  mrbc_int_t pos;

  mdir = (struct picoruby_dir*)v[0].instance->data;
  if (!mdir) {
    SET_NIL_RETURN();
    return;
  }
  if (!mdir->dir) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "closed directory");
    return;
  }
  pos = (mrbc_int_t)telldir(mdir->dir);
  SET_INT_RETURN(pos);
#endif
}

void
mrbc_dir_init(mrbc_vm *vm)
{
  mrbc_class *d = mrbc_define_class(vm, "Dir", mrbc_class_object);

  mrbc_define_method(vm, d, "new",    c_dir_init);
  mrbc_define_method(vm, d, "delete", c_dir_delete);
  mrbc_define_method(vm, d, "rmdir",  c_dir_delete);
  mrbc_define_method(vm, d, "unlink", c_dir_delete);
  mrbc_define_method(vm, d, "exist?", c_dir_exist_q);
  mrbc_define_method(vm, d, "getwd",  c_dir_getwd);
  mrbc_define_method(vm, d, "pwd",    c_dir_getwd);
  mrbc_define_method(vm, d, "mkdir",  c_dir_mkdir);
  mrbc_define_method(vm, d, "_chdir", c_dir__chdir);
  mrbc_define_method(vm, d, "chroot", c_dir_chroot);
  mrbc_define_method(vm, d, "empty?", c_dir_empty_q);

  mrbc_define_method(vm, d, "close",      c_dir_close);
  mrbc_define_method(vm, d, "read",       c_dir_read);
  mrbc_define_method(vm, d, "rewind",     c_dir_rewind);
  mrbc_define_method(vm, d, "seek",       c_dir_seek);
  mrbc_define_method(vm, d, "pos=",       c_dir_seek);
  mrbc_define_method(vm, d, "tell",       c_dir_tell);
  mrbc_define_method(vm, d, "pos",        c_dir_tell);
}

