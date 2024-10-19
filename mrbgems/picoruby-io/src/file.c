#include <mrubyc.h>
#include "../include/io.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
  #include <windows.h>
  #include <io.h>
  #define NULL_FILE "NUL"
  #define UNLINK _unlink
  #define GETCWD _getcwd
  #define CHMOD(a, b) 0
  #define MAXPATHLEN 1024
 #if !defined(PATH_MAX)
  #define PATH_MAX _MAX_PATH
 #endif
  #define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
  #include <direct.h>
#else
  #define NULL_FILE "/dev/null"
  #include <unistd.h>
  #define UNLINK unlink
  #define GETCWD getcwd
  #define CHMOD(a, b) chmod(a,b)
  #include <sys/file.h>
#ifndef __DJGPP__
  #include <libgen.h>
#endif
  #include <sys/param.h>
  #include <pwd.h>
#endif

#define FILE_SEPARATOR "/"

#if defined(_WIN32) || defined(_WIN64)
  #define PATH_SEPARATOR ";"
  #define FILE_ALT_SEPARATOR "\\"
  #define VOLUME_SEPARATOR ":"
#else
  #define PATH_SEPARATOR ":"
#endif

#ifndef LOCK_SH
#define LOCK_SH 1
#endif
#ifndef LOCK_EX
#define LOCK_EX 2
#endif
#ifndef LOCK_NB
#define LOCK_NB 4
#endif
#ifndef LOCK_UN
#define LOCK_UN 8
#endif

#if !defined(_WIN32) || defined(PICORB_MINGW32_LEGACY)
typedef struct stat         picorb_stat;
# define picorb_stat(path, sb) stat(path, sb)
# define picorb_fstat(fd, sb)  fstat(fd, sb)
#elif defined PICORB_INT32
typedef struct _stat32      picorb_stat;
# define picorb_stat(path, sb) _stat32(path, sb)
# define picorb_fstat(fd, sb)  _fstat32(fd, sb)
#else
typedef struct _stat64      picorb_stat;
# define picorb_stat(path, sb) _stat64(path, sb)
# define picorb_fstat(fd, sb)  _fstat64(fd, sb)
#endif

#ifdef _WIN32
static int
flock(int fd, int operation) {
  OVERLAPPED ov;
  HANDLE h = (HANDLE)_get_osfhandle(fd);
  DWORD flags;
  flags = ((operation & LOCK_NB) ? LOCKFILE_FAIL_IMMEDIATELY : 0)
          | ((operation & LOCK_SH) ? LOCKFILE_EXCLUSIVE_LOCK : 0);
  ov = (OVERLAPPED){0};
  return LockFileEx(h, flags, 0, 0xffffffff, 0xffffffff, &ov) ? 0 : -1;
}
#endif

#define picorb_locale_from_utf8(p, l) ((char *)(p))
#define picorb_utf8_from_locale(p, l) ((char *)(p))
#define picorb_locale_free(p)  // no-op
#define picorb_utf8_free(p)    // no-op
#define RSTRING_CSTR(vm, s)   ((char *)(s.string->data))

static void
c_file_umask(mrbc_vm *vm, mrbc_value v[], int argc)
{
#if defined(_WIN32) || defined(_WIN64)
  /* nothing to do on windows */
  SET_INT_RETURN(0);

#else
  mrbc_int_t omask;
  if (argc == 0) {
    omask = umask(0);
    umask(omask);
  }
  else {
    omask = umask(GET_INT_ARG(1));
  }
  SET_INT_RETURN(omask);
#endif
}

static void
c_file_unlink(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t i;

  for (i = 0; i < argc; i++) {
    mrbc_value pathv = GET_ARG(i);
    if (pathv.tt != MRBC_TT_STRING) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
    }
    const char *utf8_path = (const char *)GET_STRING_ARG(i);
    char *path = picorb_locale_from_utf8(utf8_path, -1);
    if (UNLINK(path) < 0) {
      picorb_locale_free(path);
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "unlink failed");
      return;
    }
    picorb_locale_free(path);
  }
  SET_INT_RETURN(argc);
}

static void
c_file_rename(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value from, to;
  char *src, *dst;

  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  from = GET_ARG(1);
  to = GET_ARG(2);
  src = picorb_locale_from_utf8(RSTRING_CSTR(vm, from), -1);
  dst = picorb_locale_from_utf8(RSTRING_CSTR(vm, to), -1);
  if (rename(src, dst) < 0) {
#if defined(_WIN32) || defined(_WIN64)
    if (CHMOD(dst, 0666) == 0 && UNLINK(dst) == 0 && rename(src, dst) == 0) {
      picorb_locale_free(src);
      picorb_locale_free(dst);
      SET_INT_RETURN(0);
    }
#endif
    picorb_locale_free(src);
    picorb_locale_free(dst);
    mrbc_raisef(vm, MRBC_CLASS(RuntimeError), "rename failed: (%s, %s)", src, dst);
    return;
  }
  picorb_locale_free(src);
  picorb_locale_free(dst);
  SET_INT_RETURN(0);
}

static void
c_file_dirname(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
#if defined(_WIN32) || defined(_WIN64)
  char dname[_MAX_DIR], vname[_MAX_DRIVE];
  char buffer[_MAX_DRIVE + _MAX_DIR];
  const char *utf8_path;
  char *path;
  size_t ridx;
  mrbc_value result;

  utf8_path = (const char *)GET_STRING_ARG(1);

  path = picorb_locale_from_utf8(utf8_path, -1);
  _splitpath(path, vname, dname, NULL, NULL);
  snprintf(buffer, _MAX_DRIVE + _MAX_DIR, "%s%s", vname, dname);
  picorb_locale_free(path);
  ridx = strlen(buffer);
  if (ridx == 0) {
    strncpy(buffer, ".", 2);  /* null terminated */
  }
  else if (ridx > 1) {
    ridx--;
    while (ridx > 0 && (buffer[ridx] == '/' || buffer[ridx] == '\\')) {
      buffer[ridx] = '\0';  /* remove last char */
      ridx--;
    }
  }
  result = mrbc_string_new_cstr(vm, buffer);
  SET_RETURN(result);
#else
  char *dname, *path;
  mrbc_value s, result;

  s = mrbc_string_dup(vm, &GET_ARG(1));

  path = picorb_locale_from_utf8(s.string->data, -1);

  if ((dname = dirname(path)) == NULL) {
    picorb_locale_free(path);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "dirname failed");
    return;
  }
  picorb_locale_free(path);
  result = mrbc_string_new_cstr(vm, dname);
  mrbc_decref(&s);
  SET_RETURN(result);
#endif
}

static void
c_file_basename(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  // NOTE: Do not use picorb_locale_from_utf8 here
#if defined(_WIN32) || defined(_WIN64)
  char bname[_MAX_DIR];
  char extname[_MAX_EXT];
  char *path;
  size_t ridx;
  char buffer[_MAX_DIR + _MAX_EXT];
  mrbc_value result;

  path = (const char *)GET_STRING_ARG(1);

  ridx = strlen(path);
  if (ridx > 0) {
    ridx--;
    while (ridx > 0 && (path[ridx] == '/' || path[ridx] == '\\')) {
      path[ridx] = '\0';
      ridx--;
    }
    if (strncmp(path, "/", 2) == 0) {
      result = mrbc_string_new_cstr(vm, path);
      SET_RETURN(result);
    }
  }
  _splitpath((const char*)path, NULL, NULL, bname, extname);
  snprintf(buffer, _MAX_DIR + _MAX_EXT, "%s%s", bname, extname);
  return mrbc_string_new_cstr(vm, buffer);
#else
  char *bname, *path;
  mrbc_value result;

  path = (char *)GET_STRING_ARG(1);

  if ((bname = basename(path)) == NULL) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "basename failed");
    return;
  }
  if (strncmp(bname, "//", 3) == 0) bname[1] = '\0';  /* patch for Cygwin */
  result = mrbc_string_new_cstr(vm, bname);
//  mrbc_incref(&result);
  SET_RETURN(result);
#endif
}

static void
c_file_realpath(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value pathname, dir_string, s, result;
  char *cpath;

  if (argc < 1 || 2 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
  }
  pathname = GET_ARG(1);
  if (argc == 2) {
    dir_string = GET_ARG(2);
    s = mrbc_string_dup(vm, &dir_string);
    mrbc_value sep = mrbc_string_new_cstr(vm, FILE_SEPARATOR);
    mrbc_string_append(&s, &sep);
    mrbc_string_append(&s, &pathname);
    pathname = s;
  }
  cpath = picorb_locale_from_utf8(RSTRING_CSTR(vm, pathname), -1);
  char buf[PATH_MAX];
  if (realpath(cpath, buf) == NULL) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "realpath failed");
    return;
  }
  result = mrbc_string_new_cstr(vm, buf);
  SET_RETURN(result);
}

static void
c_file__getwd(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value path;
  char buf[MAXPATHLEN], *utf8;

//  vm->c->ci->mid = 0;
  if (GETCWD(buf, MAXPATHLEN) == NULL) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "getcwd failed");
    return;
  }
  utf8 = picorb_utf8_from_locale(buf, -1);
  path = mrbc_string_new_cstr(vm, utf8);
  picorb_utf8_free(utf8);
  SET_RETURN(path);
}

#ifdef _WIN32
#define IS_FILESEP(x) (x == (*(char*)(FILE_SEPARATOR)) || x == (*(char*)(FILE_ALT_SEPARATOR)))
#define IS_VOLSEP(x) (x == (*(char*)(VOLUME_SEPARATOR)))
#define IS_DEVICEID(x) (x == '.' || x == '?')
#define CHECK_UNCDEV_PATH (IS_FILESEP(path[0]) && IS_FILESEP(path[1]))

static int
is_absolute_traditional_path(const char *path, size_t len)
{
  if (len < 3) return 0;
  return (ISALPHA(path[0]) && IS_VOLSEP(path[1]) && IS_FILESEP(path[2]));
}

static int
is_absolute_unc_path(const char *path, size_t len) {
  if (len < 2) return 0;
  return (CHECK_UNCDEV_PATH && !IS_DEVICEID(path[2]));
}

static int
is_absolute_device_path(const char *path, size_t len) {
  if (len < 4) return 0;
  return (CHECK_UNCDEV_PATH && IS_DEVICEID(path[2]) && IS_FILESEP(path[3]));
}

static int
file_is_absolute_path(const char *path)
{
  size_t len = strlen(path);
  if (IS_FILESEP(path[0])) return 1;
  if (len > 0)
    return (
      is_absolute_traditional_path(path, len) ||
      is_absolute_unc_path(path, len) ||
      is_absolute_device_path(path, len)
      );
  else
    return 0;
}

#undef IS_FILESEP
#undef IS_VOLSEP
#undef IS_DEVICEID
#undef CHECK_UNCDEV_PATH

#else
static int
file_is_absolute_path(const char *path)
{
  return (path[0] == *(char*)(FILE_SEPARATOR));
}
#endif

static void
c_file__gethome(mrbc_vm *vm, mrbc_value v[], int argc)
{
  char *home;
  mrbc_value path;

//  vm->c->ci->mid = 0;
#ifndef _WIN32
  mrbc_value username = mrbc_nil_value();

  if (argc == 0) {
    home = getenv("HOME");
    if (home == NULL) {
      SET_NIL_RETURN();
      return;
    }
    if (!file_is_absolute_path(home)) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "non-absolute home");
      return;
    }
  }
  else {
    username = GET_ARG(1);
    const char *cuser = RSTRING_CSTR(vm, username);
    struct passwd *pwd = getpwnam(cuser);
    if (pwd == NULL) {
      SET_NIL_RETURN();
    }
    home = pwd->pw_dir;
    if (!file_is_absolute_path(home)) {
      mrbc_raisef(vm, MRBC_CLASS(ArgumentError), "non-absolute home of ~%s", cuser);
      return;
    }
  }
  home = picorb_locale_from_utf8(home, -1);
  path = mrbc_string_new_cstr(vm, home);
  picorb_locale_free(home);
  SET_RETURN(path);
#else  /* _WIN32 */
  if (argc == 0) {
    home = getenv("USERPROFILE");
    if (home == NULL) {
      SET_NIL_RETURN();
    }
    if (!file_is_absolute_path(home)) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "non-absolute home");
      return;
    }
  }
  else {
    SET_NIL_RETURN();
    return
  }
  home = picorb_locale_from_utf8(home, -1);
  path = mrbc_string_new_cstr(vm, home);
  picorb_locale_free(home);
  SET_RETURN(path);
#endif
}

#define TIME_OVERFLOW_P(a) (sizeof(time_t) >= sizeof(mrbc_int_t) && ((a) > INT_MAX || (a) < INT_MIN))
#define TIME_T_UINT (~(time_t)0 > 0)
#if defined(PICORB_USE_BITINT)
#define TIME_BIGTIME(vm, a) do { \
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "PICORB_USE_BITINT is not implemented"); \
  return; \
} while (0)
#elif defined(MRBC_USE_FLOAT)
#define TIME_BIGTIME(vm,a) do { \
  SET_FLOAT_RETURN((mrbc_float_t)(a)); \
  return; \
} while (0)
#else
#define TIME_BIGTIME(vm, a) do { \
  mrbc_raise(vm, MRBC_CLASS(IOError), #a " overflow"); \
  return; \
} while (0)
#endif

static void
c_file_atime(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int fd = picorb_io_fileno(vm, v[0]);
  picorb_stat st;

//  vm->c->ci->mid = 0;
  if (picorb_fstat(fd, &st) == -1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "atime failed");
    return;
  }
  if (TIME_OVERFLOW_P(st.st_atime)) {
    TIME_BIGTIME(vm, st.st_atime);
  }
  SET_INT_RETURN((mrbc_int_t)st.st_atime);
}

static void
c_file_ctime(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int fd = picorb_io_fileno(vm, v[0]);
  picorb_stat st;

//  vm->c->ci->mid = 0;
  if (picorb_fstat(fd, &st) == -1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "ctime failed");
    return;
  }
  if (TIME_OVERFLOW_P(st.st_ctime)) {
    TIME_BIGTIME(vm, st. st_ctime);
  }
  SET_INT_RETURN((mrbc_int_t)st.st_ctime);
}

static void
c_file_mtime(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int fd = picorb_io_fileno(vm, v[0]);
  picorb_stat st;

//  vm->c->ci->mid = 0;
  if (picorb_fstat(fd, &st) == -1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mtime failed");
    return;
  }
  if (TIME_OVERFLOW_P(st.st_mtime)) {
    TIME_BIGTIME(vm, st. st_mtime);
  }
  SET_INT_RETURN((mrbc_int_t)st.st_mtime);
}

static void
c_file_flock(mrbc_vm *vm, mrbc_value v[], int argc)
{
#if defined(sun)
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "flock is not supported on Illumos/Solaris/Windows");
  return;
#else
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_int_t operation = GET_INT_ARG(1);
  int fd;

  fd = picorb_io_fileno(vm, v[0]);

  while (flock(fd, operation) == -1) {
    switch (errno) {
      case EINTR:
        /* retry */
        break;
      case EAGAIN:      /* NetBSD */
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
      case EWOULDBLOCK: /* FreeBSD OpenBSD Linux */
#endif
        if (operation & LOCK_NB) {
          SET_FALSE_RETURN();
          return;
        }
        /* FALLTHRU - should not happen */
      default:
        mrbc_raisef(vm, MRBC_CLASS(IOError), "flock failed: %s", strerror(errno));
        return;
    }
  }
#endif
  SET_INT_RETURN(0);
}

static void
c_file__size(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_stat st;
  int fd;

  fd = picorb_io_fileno(vm, v[0]);
  if (picorb_fstat(fd, &st) == -1) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "fstat failed");
    return;
  }

  if (st.st_size > INT_MAX) {
#ifndef MRBC_USE_FLOAT
    mrbc_raise(vm, MRBC_CLASS(IOError), "File#size too large for MRBC_USE_FLOAT");
    return;
#else
    SET_FLOAT_RETURN((mrbc_float_t)st.st_size);
    return;
#endif
  }

  SET_INT_RETURN(st.st_size);
}

static int
file_ftruncate(int fd, mrbc_int_t length)
{
#ifndef _WIN32
  return ftruncate(fd, (off_t)length);
#else
  HANDLE file;
  __int64 cur;

  file = (HANDLE)_get_osfhandle(fd);
  if (file == INVALID_HANDLE_VALUE) {
    return -1;
  }

  cur = _lseeki64(fd, 0, SEEK_CUR);
  if (cur == -1) return -1;

  if (_lseeki64(fd, (__int64)length, SEEK_SET) == -1) return -1;

  if (!SetEndOfFile(file)) {
    errno = EINVAL; /* TODO: GetLastError to errno */
    return -1;
  }

  if (_lseeki64(fd, cur, SEEK_SET) == -1) return -1;

  return 0;
#endif /* _WIN32 */
}

static void
c_file_truncate(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int fd;
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_int_t length = GET_INT_ARG(1);

  fd = picorb_io_fileno(vm, v[0]);
  if (file_ftruncate(fd, length) != 0) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "ftruncate failed");
    return;
  }

  SET_INT_RETURN(0);
}

static void
c_file_symlink(mrbc_vm *vm, mrbc_value v[], int argc)
{
#if defined(_WIN32) || defined(_WIN64)
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "symlink is not supported on this platform");
  return;
#else
  mrbc_value from, to;
  const char *src, *dst;

  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  from = GET_ARG(1);
  to = GET_ARG(2);

  src = picorb_locale_from_utf8(RSTRING_CSTR(vm, from), -1);
  dst = picorb_locale_from_utf8(RSTRING_CSTR(vm, to), -1);
  if (symlink(src, dst) == -1) {
    picorb_locale_free(src);
    picorb_locale_free(dst);
    mrbc_raisef(vm, MRBC_CLASS(RuntimeError), "symlink failed: (%s, %s)", src, dst);
    return;
  }
  picorb_locale_free(src);
  picorb_locale_free(dst);
#endif
  SET_INT_RETURN(0);
}

static void
c_file_chmod(mrbc_vm *vm, mrbc_value v[], int argc) {
  mrbc_int_t mode;
  mrbc_int_t i;
  mrbc_value file;

  if (argc < 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mode = GET_INT_ARG(1);

  for (i = 2; i < argc+1; i++) {
    file = GET_ARG(i);
    if (file.tt != MRBC_TT_STRING) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
      return;
    }
    const char *utf8_path = RSTRING_CSTR(vm, file);
    char *path = picorb_locale_from_utf8(utf8_path, -1);
    if (CHMOD(path, mode) == -1) {
      picorb_locale_free(path);
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "chmod failed");
      return;
    }
    picorb_locale_free(path);
  }

  SET_INT_RETURN(argc);
}

static void
c_file_readlink(mrbc_vm *vm, mrbc_value v[], int argc) {
#if defined(_WIN32) || defined(_WIN64)
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "readlink is not supported on this platform");
  return;
#else
  const char *path;
  char *buf, *tmp;
  size_t bufsize = 100;
  ssize_t rc;
  mrbc_value ret;

  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  path = (const char *)GET_STRING_ARG(1);
  tmp = picorb_locale_from_utf8(path, -1);

  buf = (char*)mrbc_alloc(vm, bufsize);
  while ((rc = readlink(tmp, buf, bufsize)) == (ssize_t)bufsize && rc != -1) {
    bufsize *= 2;
    buf = (char*)mrbc_realloc(vm, buf, bufsize);
  }
  picorb_locale_free(tmp);
  if (rc == -1) {
    mrbc_free(vm, buf);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "readlink failed");
    return;
  }
  tmp = picorb_utf8_from_locale(buf, -1);
  ret = mrbc_string_new(vm, tmp, rc);
  picorb_locale_free(tmp);
  mrbc_free(vm, buf);

  SET_RETURN(ret);
#endif
}

#define PICORB_SYM(name) mrbc_str_to_symid(#name)

void
mrbc_io_file_init(mrbc_vm *vm, mrbc_class *io)
{
  mrbc_class *file, *cnst;

  file = mrbc_define_class(vm, "File", io);

  mrbc_define_method(vm, file, "umask",     c_file_umask);
  mrbc_define_method(vm, file, "delete",    c_file_unlink);
  mrbc_define_method(vm, file, "unlink",    c_file_unlink);
  mrbc_define_method(vm, file, "rename",    c_file_rename);
  mrbc_define_method(vm, file, "symlink",   c_file_symlink);
  mrbc_define_method(vm, file, "chmod",     c_file_chmod);
  mrbc_define_method(vm, file, "readlink",  c_file_readlink);

  mrbc_define_method(vm, file, "dirname",   c_file_dirname);
  mrbc_define_method(vm, file, "basename",  c_file_basename);
  mrbc_define_method(vm, file, "realpath",  c_file_realpath);
  mrbc_define_method(vm, file, "_getwd",    c_file__getwd);
  mrbc_define_method(vm, file, "_gethome",  c_file__gethome);

  mrbc_define_method(vm, file, "flock",     c_file_flock);
  mrbc_define_method(vm, file, "_atime",    c_file_atime);
  mrbc_define_method(vm, file, "_ctime",    c_file_ctime);
  mrbc_define_method(vm, file, "_mtime",    c_file_mtime);
  mrbc_define_method(vm, file, "_size",     c_file__size);
  mrbc_define_method(vm, file, "truncate",  c_file_truncate);

  cnst = mrbc_define_module_under(vm, file, "Constants");
  mrbc_value str;
  mrbc_set_class_const(cnst, PICORB_SYM(LOCK_SH), &mrbc_integer_value(LOCK_SH));
  mrbc_set_class_const(cnst, PICORB_SYM(LOCK_EX), &mrbc_integer_value(LOCK_EX));
  mrbc_set_class_const(cnst, PICORB_SYM(LOCK_UN), &mrbc_integer_value(LOCK_UN));
  mrbc_set_class_const(cnst, PICORB_SYM(LOCK_NB), &mrbc_integer_value(LOCK_NB));
  str = mrbc_string_new_cstr(vm, "FILE_SEPARATOR");
  mrbc_set_class_const(cnst, PICORB_SYM(SEPARATOR), &str);
  str = mrbc_string_new_cstr(vm, "PATH_SEPARATOR");
  mrbc_set_class_const(cnst, PICORB_SYM(PATH_SEPARATOR), &str);
#if defined(_WIN32) || defined(_WIN64)
  str = mrbc_string_new_cstr(vm, FILE_ALT_SEPARATOR);
  mrbc_set_class_const(cnst, PICORB_SYM(ALT_SEPARATOR), &str);
#else
  mrbc_set_class_const(cnst, PICORB_SYM(ALT_SEPARATOR), &mrbc_nil_value());
#endif
  str = mrbc_string_new_cstr(vm, NULL_FILE);
  mrbc_set_class_const(cnst, PICORB_SYM(NULL), &str);

  mrbc_set_class_const(cnst, PICORB_SYM(RDONLY), &mrbc_integer_value(PICORB_O_RDONLY));
  mrbc_set_class_const(cnst, PICORB_SYM(WRONLY), &mrbc_integer_value(PICORB_O_WRONLY));
  mrbc_set_class_const(cnst, PICORB_SYM(RDWR), &mrbc_integer_value(PICORB_O_RDWR));
  mrbc_set_class_const(cnst, PICORB_SYM(APPEND), &mrbc_integer_value(PICORB_O_APPEND));
  mrbc_set_class_const(cnst, PICORB_SYM(CREAT), &mrbc_integer_value(PICORB_O_CREAT));
  mrbc_set_class_const(cnst, PICORB_SYM(EXCL), &mrbc_integer_value(PICORB_O_EXCL));
  mrbc_set_class_const(cnst, PICORB_SYM(TRUNC), &mrbc_integer_value(PICORB_O_TRUNC));
  mrbc_set_class_const(cnst, PICORB_SYM(NONBLOCK), &mrbc_integer_value(PICORB_O_NONBLOCK));
  mrbc_set_class_const(cnst, PICORB_SYM(NOCTTY), &mrbc_integer_value(PICORB_O_NOCTTY));
  mrbc_set_class_const(cnst, PICORB_SYM(BINARY), &mrbc_integer_value(PICORB_O_BINARY));
  mrbc_set_class_const(cnst, PICORB_SYM(SHARE_DELETE), &mrbc_integer_value(PICORB_O_SHARE_DELETE));
  mrbc_set_class_const(cnst, PICORB_SYM(SYNC), &mrbc_integer_value(PICORB_O_SYNC));
  mrbc_set_class_const(cnst, PICORB_SYM(DSYNC), &mrbc_integer_value(PICORB_O_DSYNC));
  mrbc_set_class_const(cnst, PICORB_SYM(RSYNC), &mrbc_integer_value(PICORB_O_RSYNC));
  mrbc_set_class_const(cnst, PICORB_SYM(NOFOLLOW), &mrbc_integer_value(PICORB_O_NOFOLLOW));
  mrbc_set_class_const(cnst, PICORB_SYM(NOATIME), &mrbc_integer_value(PICORB_O_NOATIME));
  mrbc_set_class_const(cnst, PICORB_SYM(DIRECT), &mrbc_integer_value(PICORB_O_DIRECT));
  mrbc_set_class_const(cnst, PICORB_SYM(TMPFILE), &mrbc_integer_value(PICORB_O_TMPFILE));
}
