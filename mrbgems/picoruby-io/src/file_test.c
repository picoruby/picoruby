#include <mrubyc.h>
#include "../include/io.h"

#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
  #define LSTAT stat
  #include <winsock.h>
#else
  #define LSTAT lstat
  #include <sys/file.h>
  #include <sys/param.h>
  #include <sys/wait.h>
#ifndef __DJGPP__
  #include <libgen.h>
#endif
  #include <pwd.h>
  #include <unistd.h>
#endif

#include <fcntl.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>


static int
filetest_stat0(mrbc_vm *vm, mrbc_value obj, struct stat *st, int do_lstat)
{
  if (mrbc_obj_is_kind_of(&obj, mrbc_get_class_by_name("IO"))) {
    struct picorb_io *fptr;
    fptr = (struct picorb_io*)obj.instance->data;

    if (fptr && fptr->fd >= 0) {
      return fstat(fptr->fd, st);
    }

    mrbc_raise(vm, MRBC_CLASS(IOError), "closed stream");
    return -1;
  }
  else {
    char *path = picorb_locale_from_utf8(RSTRING_CSTR(vm, obj), -1);
    int ret;
    if (do_lstat) {
      ret = LSTAT(path, st);
    }
    else {
      ret = stat(path, st);
    }
    picorb_locale_free(path);
    return ret;
  }
}

static int
filetest_stat(mrbc_vm *vm, mrbc_value obj, struct stat *st)
{
  return filetest_stat0(vm, obj, st, 0);
}

#ifdef S_ISLNK
static int
filetest_lstat(mrbc_vm *vm, mrbc_value obj, struct stat *st)
{
  return filetest_stat0(vm, obj, st, 1);
}
#endif

/*
 * Document-method: directory?
 *
 * call-seq:
 *   File.directory?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a directory,
 * or a symlink that points at a directory, and <code>false</code>
 * otherwise.
 *
 *    File.directory?(".")
 */

static void
c_filetest_directory_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
#ifndef S_ISDIR
#   define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

  struct stat st;
  if (argc != 1)
  {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_stat(vm, obj, &st) < 0)
  {
    SET_FALSE_RETURN();
    return;
  }
  if (S_ISDIR(st.st_mode))
  {
    SET_TRUE_RETURN();
    return;
  }

  SET_FALSE_RETURN();
}

/*
 * call-seq:
 *   File.pipe?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a pipe.
 */

static void
c_filetest_pipe_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
#if defined(_WIN32) || defined(_WIN64)
  mrbc_raise(vm, MRBC_CLASS(NotImlementedError), "pipe is not supported on this platform");
#else
#ifdef S_IFIFO
#  ifndef S_ISFIFO
#    define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#  endif

  struct stat st;
  if (argc != 1)
  {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_stat(vm, obj, &st) < 0)
  {
    SET_FALSE_RETURN();
    return;
  }
  if (S_ISFIFO(st.st_mode))
  {
    SET_TRUE_RETURN();
    return;
  }

#endif
  SET_FALSE_RETURN();
#endif
}

/*
 * call-seq:
 *   File.symlink?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a symbolic link.
 */

static void
c_filetest_symlink_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
#if defined(_WIN32) || defined(_WIN64)
  mrbc_raise(vm, MRBC_CLASS(NotImlementedError), "symlink is not supported on this platform");
#else
#ifndef S_ISLNK
#  ifdef _S_ISLNK
#    define S_ISLNK(m) _S_ISLNK(m)
#  else
#    ifdef _S_IFLNK
#      define S_ISLNK(m) (((m) & S_IFMT) == _S_IFLNK)
#    else
#      ifdef S_IFLNK
#        define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#      endif
#    endif
#  endif
#endif

#ifdef S_ISLNK
  struct stat st;
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_lstat(vm, obj, &st) == -1)
  {
    SET_FALSE_RETURN();
    return;
  }
  if (S_ISLNK(st.st_mode))
  {
    SET_TRUE_RETURN();
    return;
  }

#endif

  SET_FALSE_RETURN();
#endif
}

/*
 * call-seq:
 *   File.socket?(file_name)   ->  true or false
 *
 * Returns <code>true</code> if the named file is a socket.
 */

static void
c_filetest_socket_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
#if defined(_WIN32) || defined(_WIN64)
  mrbc_raise(vm, MRBC_CLASS(NotImlementedError), "socket is not supported on this platform");
#else
#ifndef S_ISSOCK
#  ifdef _S_ISSOCK
#    define S_ISSOCK(m) _S_ISSOCK(m)
#  else
#    ifdef _S_IFSOCK
#      define S_ISSOCK(m) (((m) & S_IFMT) == _S_IFSOCK)
#    else
#      ifdef S_IFSOCK
#        define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#      endif
#    endif
#  endif
#endif

#ifdef S_ISSOCK
  struct stat st;
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_stat(vm, obj, &st) < 0)
  {
    SET_FALSE_RETURN();
    return;
  }
  if (S_ISSOCK(st.st_mode))
  {
    SET_TRUE_RETURN();
    return;
  }
#endif

  SET_FALSE_RETURN();
#endif
}

/*
 * call-seq:
 *    File.exist?(file_name)    ->  true or false
 *
 * Return <code>true</code> if the named file exists.
 */

static void
c_filetest_exist_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct stat st;
  if (argc != 1)
  {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_stat(vm, obj, &st) < 0)
  {
    SET_FALSE_RETURN();
    return;
  }

  SET_TRUE_RETURN();
}

/*
 * call-seq:
 *    File.file?(file_name)   -> true or false
 *
 * Returns <code>true</code> if the named file exists and is a
 * regular file.
 */

static void
c_filetest_file_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
#ifndef S_ISREG
#   define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

  struct stat st;
  if (argc != 1)
  {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_stat(vm, obj, &st) < 0)
  {
    SET_FALSE_RETURN();
    return;
  }
  if (S_ISREG(st.st_mode))
  {
    SET_TRUE_RETURN();
    return;
  }

  SET_FALSE_RETURN();
}

/*
 * call-seq:
 *    File.zero?(file_name)   -> true or false
 *
 * Returns <code>true</code> if the named file exists and has
 * a zero size.
 */

static void
c_filetest_zero_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct stat st;
  if (argc != 1)
  {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_stat(vm, obj, &st) < 0)
  {
    SET_FALSE_RETURN();
    return;
  }
  if (st.st_size == 0)
  {
    SET_TRUE_RETURN();
    return;
  }

  SET_FALSE_RETURN();
}

/*
 * call-seq:
 *    File.size(file_name)   -> integer
 *
 * Returns the size of <code>file_name</code>.
 *
 * _file_name_ can be an IO object.
 */

static void
c_filetest_size(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct stat st;
  if (argc != 1)
  {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_stat(vm, obj, &st) < 0)
  {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "filetest_stat");
    return;
  }

  SET_INT_RETURN(st.st_size);
}

/*
 * call-seq:
 *    File.size?(file_name)   -> Integer or nil
 *
 * Returns +nil+ if +file_name+ doesn't exist or has zero size, the size of the
 * file otherwise.
 */

static void
c_filetest_size_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct stat st;
  if (argc != 1)
  {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value obj = GET_ARG(1);

  if (filetest_stat(vm, obj, &st) < 0)
  {
    SET_NIL_RETURN();
    return;
  }
  if (st.st_size == 0)
  {
    SET_NIL_RETURN();
    return;
  }

  SET_INT_RETURN(st.st_size);
}

void
mrbc_io_file_test_init(mrbc_vm *vm)
{
  mrbc_class *file_test = mrbc_define_class(vm, "FileTest", mrbc_class_object);

  mrbc_define_method(vm, file_test, "directory?", c_filetest_directory_p);
  mrbc_define_method(vm, file_test, "exist?",     c_filetest_exist_p);
  mrbc_define_method(vm, file_test, "file?",      c_filetest_file_p);
  mrbc_define_method(vm, file_test, "pipe?",      c_filetest_pipe_p);
  mrbc_define_method(vm, file_test, "size",       c_filetest_size);
  mrbc_define_method(vm, file_test, "size?",      c_filetest_size_p);
  mrbc_define_method(vm, file_test, "socket?",    c_filetest_socket_p);
  mrbc_define_method(vm, file_test, "symlink?",   c_filetest_symlink_p);
  mrbc_define_method(vm, file_test, "zero?",      c_filetest_zero_p);
}

