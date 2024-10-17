#include <mrubyc.h>
#include "../include/io.h"

#include <sys/types.h>
#include <sys/stat.h>

#if defined(_WIN32) || defined(_WIN64)
  #include <winsock.h>
  #include <io.h>
  #include <basetsd.h>
  #define open  _open
  #define close _close
  #define dup _dup
  #define dup2 _dup2
  #define read  _read
  #define write _write
  #define lseek _lseek
  #define isatty _isatty
  #define WEXITSTATUS(x) (x)
  typedef int fsize_t;
  typedef long ftime_t;
  typedef long fsuseconds_t;
  typedef int fmode_t;
  typedef int fssize_t;

  #ifndef O_TMPFILE
    #define O_TMPFILE O_TEMPORARY
  #endif
#else
  #include <sys/wait.h>
  #include <sys/time.h>
  #include <unistd.h>
  typedef size_t fsize_t;
  typedef time_t ftime_t;
#ifdef __DJGPP__
  typedef long fsuseconds_t;
#else
  typedef suseconds_t fsuseconds_t;
#endif
  typedef mode_t fmode_t;
  typedef ssize_t fssize_t;
#endif

#include <fcntl.h>

#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#define OPEN_ACCESS_MODE_FLAGS (O_RDONLY | O_WRONLY | O_RDWR)
#define OPEN_RDONLY_P(f)       ((bool)(((f) & OPEN_ACCESS_MODE_FLAGS) == O_RDONLY))
#define OPEN_WRONLY_P(f)       ((bool)(((f) & OPEN_ACCESS_MODE_FLAGS) == O_WRONLY))
#define OPEN_RDWR_P(f)         ((bool)(((f) & OPEN_ACCESS_MODE_FLAGS) == O_RDWR))
#define OPEN_READABLE_P(f)     ((bool)(OPEN_RDONLY_P(f) || OPEN_RDWR_P(f)))
#define OPEN_WRITABLE_P(f)     ((bool)(OPEN_WRONLY_P(f) || OPEN_RDWR_P(f)))

#ifndef PICORB_NO_IO_POPEN
static void
io_set_process_status(mrbc_vm *vm, pid_t pid, int status)
{
  // TODO
//  struct RClass *c_status = NULL;
//  mrb_value v;
//
//  if (mrb_class_defined_id(mrb, MRB_SYM(Process))) {
//    struct RClass *c_process = mrb_module_get_id(mrb, MRB_SYM(Process));
//    if (mrb_const_defined(mrb, mrb_obj_value(c_process), MRB_SYM(Status))) {
//      c_status = mrb_class_get_under_id(mrb, c_process, MRB_SYM(Status));
//    }
//  }
//  if (c_status != NULL) {
//    v = mrb_funcall_id(mrb, mrb_obj_value(c_status), MRB_SYM(new), 2, mrb_fixnum_value(pid), mrb_fixnum_value(status));
//  }
//  else {
//    v = mrb_fixnum_value(WEXITSTATUS(status));
//  }
//  mrb_gv_set(mrb, mrb_intern_lit(mrb, "$?"), v);
}
#endif

static struct picorb_io*
io_data_get_ptr(mrbc_value io)
{
  return (struct picorb_io*)io.instance->data;
}

static struct picorb_io*
io_get_open_fptr(mrbc_vm *vm, mrbc_value io)
{
  struct picorb_io *fptr;

  fptr = (struct picorb_io*)io_data_get_ptr(io);
  if (fptr == NULL) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "uninitialized stream");
    return NULL;
  }
  if (fptr->fd < 0) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "closed stream");
    return NULL;
  }
  return fptr;
}

static int
io_modestr_to_flags(mrbc_vm *vm, const char *mode)
{
  int flags;
  const char *m = mode;

  switch (*m++) {
    case 'r':
      flags = O_RDONLY;
      break;
    case 'w':
      flags = O_WRONLY | O_CREAT | O_TRUNC;
      break;
    case 'a':
      flags = O_WRONLY | O_CREAT | O_APPEND;
      break;
    default:
      goto modeerr;
  }

  while (*m) {
    switch (*m++) {
      case 'b':
#ifdef O_BINARY
        flags |= O_BINARY;
#endif
        break;
      case 'x':
        if (mode[0] != 'w') goto modeerr;
        flags |= O_EXCL;
        break;
      case '+':
        flags = (flags & ~OPEN_ACCESS_MODE_FLAGS) | O_RDWR;
        break;
      case ':':
        /* XXX: PASSTHROUGH*/
      default:
        goto modeerr;
    }
  }

  return flags;

 modeerr:
  mrbc_raisef(vm, MRBC_CLASS(ArgumentError), "illegal access mode %s", mode);
  return 0; /* not reached */
}

static void
io_init_buf(mrbc_vm *vm, struct picorb_io *fptr)
{
  if (fptr->readable) {
    fptr->buf = (struct picorb_io_buf*)mrbc_alloc(vm, sizeof(struct picorb_io_buf));
    fptr->buf->start = 0;
    fptr->buf->len = 0;
  }
}

static int
io_mode_to_flags(mrbc_vm *vm, mrbc_value mode)
{
  if (mode.tt == MRBC_TT_NIL) {
    return O_RDONLY;
  }
  else if (mode.tt == MRBC_TT_STRING) {
    return io_modestr_to_flags(vm, (const char *)mode.string->data);
  }
  else {
    int flags = 0;
    int flags0 = mode.i;

    switch (flags0 & PICORB_O_ACCMODE) {
      case PICORB_O_RDONLY:
        flags |= O_RDONLY;
        break;
      case PICORB_O_WRONLY:
        flags |= O_WRONLY;
        break;
      case PICORB_O_RDWR:
        flags |= O_RDWR;
        break;
      default:
        mrbc_raise(vm, MRBC_CLASS(ArgumentError), "illegal access mode");
    }

    if (flags0 & PICORB_O_APPEND)        flags |= O_APPEND;
    if (flags0 & PICORB_O_CREAT)         flags |= O_CREAT;
    if (flags0 & PICORB_O_EXCL)          flags |= O_EXCL;
    if (flags0 & PICORB_O_TRUNC)         flags |= O_TRUNC;
#ifdef O_NONBLOCK
    if (flags0 & PICORB_O_NONBLOCK)      flags |= O_NONBLOCK;
#endif
#ifdef O_NOCTTY
    if (flags0 & PICORB_O_NOCTTY)        flags |= O_NOCTTY;
#endif
#ifdef O_BINARY
    if (flags0 & PICORB_O_BINARY)        flags |= O_BINARY;
#endif
#ifdef O_SHARE_DELETE
    if (flags0 & PICORB_O_SHARE_DELETE)  flags |= O_SHARE_DELETE;
#endif
#ifdef O_SYNC
    if (flags0 & PICORB_O_SYNC)          flags |= O_SYNC;
#endif
#ifdef O_DSYNC
    if (flags0 & PICORB_O_DSYNC)         flags |= O_DSYNC;
#endif
#ifdef O_RSYNC
    if (flags0 & PICORB_O_RSYNC)         flags |= O_RSYNC;
#endif
#ifdef O_NOFOLLOW
    if (flags0 & PICORB_O_NOFOLLOW)      flags |= O_NOFOLLOW;
#endif
#ifdef O_NOATIME
    if (flags0 & PICORB_O_NOATIME)       flags |= O_NOATIME;
#endif
#ifdef O_DIRECT
    if (flags0 & PICORB_O_DIRECT)        flags |= O_DIRECT;
#endif
#ifdef O_TMPFILE
    if (flags0 & PICORB_O_TMPFILE)       flags |= O_TMPFILE;
#endif

    return flags;
  }
}

static void
fptr_finalize(mrbc_vm *vm, struct picorb_io *fptr, int quiet)
{
  int saved_errno = 0;
  int limit = quiet ? 3 : 0;

  if (fptr == NULL) {
    return;
  }

  if (fptr->fd >= limit) {
#ifdef _WIN32
    if (fptr->is_socket) {
      if (fptr->close_fd && closesocket(fptr->fd) != 0) {
        saved_errno = WSAGetLastError();
      }
      fptr->fd = -1;
    }
#endif
    if (fptr->fd != -1 && fptr->close_fd) {
      if (close(fptr->fd) == -1) {
        saved_errno = errno;
      }
    }
    fptr->fd = -1;
  }

  if (fptr->fd2 >= limit) {
    if (fptr->close_fd2 && close(fptr->fd2) == -1) {
      if (saved_errno == 0) {
        saved_errno = errno;
      }
    }
    fptr->fd2 = -1;
  }

#ifndef PICORB_NO_IO_POPEN
  if (fptr->pid != 0) {
#if !defined(_WIN32) && !defined(_WIN64)
    pid_t pid;
    int status;
    do {
      pid = waitpid(fptr->pid, &status, 0);
    } while (pid == -1 && errno == EINTR);
    if (!quiet && pid == fptr->pid) {
      io_set_process_status(vm, pid, status);
    }
#else
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, fptr->pid);
    DWORD status;
    if (WaitForSingleObject(h, INFINITE) && GetExitCodeProcess(h, &status))
      if (!quiet)
        io_set_process_status(vm, fptr->pid, (int)status);
    CloseHandle(h);
#endif
    fptr->pid = 0;
    /* Note: we don't raise an exception when waitpid(3) fails */
  }
#endif

  if (fptr->buf) {
    mrbc_free(vm, fptr->buf);
    fptr->buf = NULL;
  }

  if (!quiet && saved_errno != 0) {
    errno = saved_errno;
    //mrb_sys_fail(mrb, "fptr_finalize failed");
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "fptr_finalize failed");
  }
}

static void
io_alloc(struct picorb_io *fptr)
{
  fptr->fd = -1;
  fptr->fd2 = -1;
  fptr->pid = 0;
  fptr->buf = 0;
  fptr->readable = 0;
  fptr->writable = 0;
  fptr->sync = 0;
  fptr->eof = 0;
  fptr->is_socket = 0;
  fptr->close_fd = 1;
  fptr->close_fd2 = 1;
}

#ifndef NOFILE
#define NOFILE 64
#endif

static struct picorb_io *
io_get_write_fptr(mrbc_vm *vm, mrbc_value io)
{
  struct picorb_io *fptr = io_get_open_fptr(vm, io);
  if (fptr == NULL) return NULL; /* raise error */
  if (!fptr->writable) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "not opened for writing");
    return NULL;
  }
  return fptr;
}

static int
io_get_write_fd(struct picorb_io *fptr)
{
  if (fptr->fd2 == -1) {
    return fptr->fd;
  }
  else {
    return fptr->fd2;
  }
}

static int
fd_write(mrbc_vm *vm, int fd, mrbc_value str)
{
  fssize_t len, sum, n;

  len = (fssize_t)(str.string->size);
  if (len == 0)return 0;

  for (sum=0; sum<len; sum+=n) {
    n = write(fd, str.string->data, (fsize_t)len);
    if (n == -1) {
      //mrb_sys_fail(mrb, "syswrite");
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "syswrite failed");
    }
  }
  return len;
}

int
picorb_io_fileno(mrbc_vm *vm, mrbc_value io)
{
  struct picorb_io *fptr;
  fptr = io_get_open_fptr(vm, io);
  return fptr->fd;
}

static int
option_to_fd(mrbc_vm *vm, mrbc_value v)
{
#ifdef PICORB_NO_IO_POPEN
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "IO.popen is not available");
#else
  if (v.tt == MRBC_TT_EMPTY) return -1;
  if (v.tt == MRBC_TT_NIL) return -1;

  switch (v.tt) {
    case MRBC_TT_OBJECT:
      if (v.instance->cls->sym_id == mrbc_str_to_symid("IO")) {
        return picorb_io_fileno(vm, v);
      }
      goto WRONG;
    case MRBC_TT_INTEGER:
      return (int)mrbc_integer(v);
    default:
    WRONG:
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong exec redirect action");
      break;
  }
#endif
  return -1; /* never reached */
}

static void
io_fd_cloexec(mrbc_vm *vm, int fd)
{
#if defined(F_GETFD) && defined(F_SETFD) && defined(FD_CLOEXEC)
  int flags, flags2;

  flags = fcntl(fd, F_GETFD);
  if (flags < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cloexec GETFD failed");
    return;
  }
  if (fd <= 2) {
    flags2 = flags & ~FD_CLOEXEC; /* Clear CLOEXEC for standard file descriptors: 0, 1, 2. */
  }
  else {
    flags2 = flags | FD_CLOEXEC; /* Set CLOEXEC for non-standard file descriptors: 3, 4, 5, ... */
  }
  if (flags != flags2) {
    if (fcntl(fd, F_SETFD, flags2) < 0) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "cloexec SETFD failed");
    }
  }
#endif
}

static int
io_process_exec(const char *pname)
{
  const char *s;
  s = pname;

  while (*s == ' ' || *s == '\t' || *s == '\n')
    s++;

  if (!*s) {
    errno = ENOENT;
    return -1;
  }

  execl("/bin/sh", "sh", "-c", pname, (char*)NULL);
  return -1;
}

/* class methods */

static void
c_io_new(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct picorb_io *fptr;
  int fd;
  mrbc_value mode;
  int flags;

  if (v[argc + 1].tt == MRBC_TT_PROC) {
    mrbc_raise(vm, MRBC_CLASS(StandardError), "IO.new does not take block; use IO.open instead");
    return;
  }

  mode = GET_ARG(2);

  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "file descriptor should be an integer");
    return;
  }
  fd = GET_INT_ARG(1);
  switch (fd) {
    case 0: /* STDIN */
    case 1: /* STDOUT */
    case 2: /* STDERR */
      break;
    default:
      // TODO check_file_descriptor(fd);
      break;
  }
  flags = io_mode_to_flags(vm, mode);

  mrbc_value io = mrbc_instance_new(vm, v->cls, sizeof(struct picorb_io));
  fptr = (struct picorb_io *)io.instance->data;
  memset(fptr, 0, sizeof(struct picorb_io));
  fptr_finalize(vm, fptr, true);
  io_alloc(fptr);

  fptr->fd = (int)fd;
  fptr->readable = OPEN_READABLE_P(flags);
  fptr->writable = OPEN_WRITABLE_P(flags);
  io_init_buf(vm, fptr);

  SET_RETURN(io);
}

static void
c_io__popen(mrbc_vm *vm, mrbc_value v[], int argc)
{
#ifdef _WIN32
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "IO.popen is not implemented for Windows");
#else
  mrbc_value io, result;
  int doexec;
  int opt_in, opt_out, opt_err;
  const char *cmd = (const char *)GET_STRING_ARG(1);
  mrbc_value mode = GET_ARG(2);

  struct picorb_io *fptr;
  int pid, flags, fd, write_fd = -1;
  int pr[2] = { -1, -1 };
  int pw[2] = { -1, -1 };
  int saved_errno;

  flags = io_mode_to_flags(vm, mode);
  doexec = (strcmp("-", cmd) != 0);
  opt_in = option_to_fd(vm, GET_ARG(3));
  opt_out = option_to_fd(vm, GET_ARG(4));
  opt_err = option_to_fd(vm, GET_ARG(5));

  io = mrbc_instance_new(vm, v->cls, sizeof(struct picorb_io));
  memset(io.instance->data, 0, sizeof(struct picorb_io));

  if (OPEN_READABLE_P(flags)) {
    if (pipe(pr) == -1) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "pipe failed");
    }
    io_fd_cloexec(vm, pr[0]);
    io_fd_cloexec(vm, pr[1]);
  }

  if (OPEN_WRITABLE_P(flags)) {
    if (pipe(pw) == -1) {
      if (pr[0] != -1) close(pr[0]);
      if (pr[1] != -1) close(pr[1]);
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "pipe failed");
    }
    io_fd_cloexec(vm, pw[0]);
    io_fd_cloexec(vm, pw[1]);
  }

  if (vm->exception.tt == MRBC_TT_EXCEPTION) {
    if (pr[0] != -1) close(pr[0]);
    if (pr[1] != -1) close(pr[1]);
    if (pw[0] != -1) close(pw[0]);
    if (pw[1] != -1) close(pw[1]);
    return;
  }

  if (!doexec) {
    fflush(stdout);
    fflush(stderr);
  }

  result = mrbc_nil_value();
  switch (pid = fork()) {
    case 0: /* child */
      if (opt_in != -1) {
        dup2(opt_in, 0);
      }
      if (opt_out != -1) {
        dup2(opt_out, 1);
      }
      if (opt_err != -1) {
        dup2(opt_err, 2);
      }
      if (OPEN_READABLE_P(flags)) {
        close(pr[0]);
        if (pr[1] != 1) {
          dup2(pr[1], 1);
          close(pr[1]);
        }
      }
      if (OPEN_WRITABLE_P(flags)) {
        close(pw[1]);
        if (pw[0] != 0) {
          dup2(pw[0], 0);
          close(pw[0]);
        }
      }
      if (doexec) {
        for (fd = 3; fd < NOFILE; fd++) {
          close(fd);
        }
        io_process_exec(cmd);
        mrbc_raisef(vm, MRBC_CLASS(IOError), "command not found: %s", cmd);
        _exit(127);
      }
      result = mrbc_nil_value();
      break;

    default: /* parent */
      if (OPEN_RDWR_P(flags)) {
        close(pr[1]);
        fd = pr[0];
        close(pw[0]);
        write_fd = pw[1];
      }
      else if (OPEN_RDONLY_P(flags)) {
        close(pr[1]);
        fd = pr[0];
      }
      else {
        close(pw[0]);
        fd = pw[1];
      }

      fptr = (struct picorb_io *)io.instance->data;
      io_alloc(fptr);
      fptr->fd = fd;
      fptr->fd2 = write_fd;
      fptr->pid = pid;
      fptr->readable = OPEN_READABLE_P(flags);
      fptr->writable = OPEN_WRITABLE_P(flags);
      io_init_buf(vm, fptr);

      result = io;
      break;

    case -1: /* error */
      saved_errno = errno;
      if (OPEN_READABLE_P(flags)) {
        close(pr[0]);
        close(pr[1]);
      }
      if (OPEN_WRITABLE_P(flags)) {
        close(pw[0]);
        close(pw[1]);
      }
      errno = saved_errno;
      //mrb_sys_fail(mrb, "pipe_open failed");
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "pipe_open failed");
      break;
  }
  SET_RETURN(result);
#endif /* _WIN32 */
}

/* instance methods */

static void
c_io_write(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct picorb_io *fptr = io_get_write_fptr(vm, v[0]);
  if (fptr == NULL) return; /* raise error */
  int fd = io_get_write_fd(fptr);
  int len = 0;

  if (fptr->buf && fptr->buf->len > 0) {
    off_t n;

    /* get current position */
    n = lseek(fd, 0, SEEK_CUR);
    //if (n == -1) mrb_sys_fail(mrb, "lseek");
    if (n == -1) mrbc_raise(vm, MRBC_CLASS(RuntimeError), "lseek failed");
    /* move cursor */
    n = lseek(fd, n - fptr->buf->len, SEEK_SET);
    //if (n == -1) mrb_sys_fail(mrb, "lseek(2)");
    if (n == -1) mrbc_raise(vm, MRBC_CLASS(RuntimeError), "lseek(2) failed");
    fptr->buf->start = fptr->buf->len = 0;
  }

  for (int i = 1; i < argc + 1; i++) {
    len += fd_write(vm, fd, v[i]);
    if (vm->exception.tt == MRBC_TT_EXCEPTION) {
      return; /* raise error */
    }
  }
  SET_INT_RETURN(len);
}

static void
c_io_closed_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct picorb_io *fptr;
  fptr = (struct picorb_io*)io_data_get_ptr(v[0]);
  if (fptr == NULL || fptr->fd >= 0) {
    SET_FALSE_RETURN();
  }
  SET_TRUE_RETURN();
}

static void
c_io_close(mrbc_vm *vm, mrbc_value v[], int argc)
{
  struct picorb_io *fptr;
  fptr = io_get_open_fptr(vm, v[0]);
  fptr_finalize(vm, fptr, false);
  SET_NIL_RETURN();
}

void
mrbc_io_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_IO = mrbc_define_class(vm, "IO", mrbc_class_object);

  // class methods
  mrbc_define_method(vm, mrbc_class_IO, "new", c_io_new);
  mrbc_define_method(vm, mrbc_class_IO, "_popen", c_io__popen);
//  mrbc_define_method(vm, mrbc_class_IO, "_sysclose", c_io__sysclose);
//  mrbc_define_method(vm, mrbc_class_IO, "for_fd", c_io_for_fd);
//  mrbc_define_method(vm, mrbc_class_IO, "select", c_io_select);
//  mrbc_define_method(vm, mrbc_class_IO, "sysopen", c_io_sysopen);
//#if !defined(_WIN32) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
//  mrbc_define_method(vm, mrbc_class_IO, "_pipe", c_io__pipe);
//#endif
//
//  // instance methods
//  mrbc_define_method(vm, mrbc_class_IO, "isatty",         c_io_isatty);
//  mrbc_define_method(vm, mrbc_class_IO, "eof?",           c_io_eof_q);
//  mrbc_define_method(vm, mrbc_class_IO, "getc",           c_io_getc);
//  mrbc_define_method(vm, mrbc_class_IO, "gets",           c_io_gets);
//  mrbc_define_method(vm, mrbc_class_IO, "read",           c_io_read);
//  mrbc_define_method(vm, mrbc_class_IO, "readchar",       c_io_readchar);
//  mrbc_define_method(vm, mrbc_class_IO, "readline",       c_io_readline);
//  mrbc_define_method(vm, mrbc_class_IO, "readlines",      c_io_readlines);
//  mrbc_define_method(vm, mrbc_class_IO, "sync",           c_io_sync);
//  mrbc_define_method(vm, mrbc_class_IO, "sync=",          c_io_sync_eq);
//  mrbc_define_method(vm, mrbc_class_IO, "sysread",        c_io_sysread);
//  mrbc_define_method(vm, mrbc_class_IO, "sysseek",        c_io_sysseek);
//  mrbc_define_method(vm, mrbc_class_IO, "syswrite",       c_io_syswrite);
//  mrbc_define_method(vm, mrbc_class_IO, "seek",           c_io_seek);
  mrbc_define_method(vm, mrbc_class_IO, "close",          c_io_close);
//  mrbc_define_method(vm, mrbc_class_IO, "close_write",    c_io_close_write);
//  mrbc_define_method(vm, mrbc_class_IO, "close_on_exec=", c_io_close_on_exec_eq);
//  mrbc_define_method(vm, mrbc_class_IO, "close_on_exec?", c_io_close_on_exec_q);
  mrbc_define_method(vm, mrbc_class_IO, "closed?",        c_io_closed_q);
//  mrbc_define_method(vm, mrbc_class_IO, "flush",          c_io_flush);
//  mrbc_define_method(vm, mrbc_class_IO, "ungetc",         c_io_ungetc);
//  mrbc_define_method(vm, mrbc_class_IO, "pos",            c_io_pos);
//  mrbc_define_method(vm, mrbc_class_IO, "pid",            c_io_pid);
//  mrbc_define_method(vm, mrbc_class_IO, "fileno",         c_io_fileno);
  mrbc_define_method(vm, mrbc_class_IO, "write",          c_io_write);
//  mrbc_define_method(vm, mrbc_class_IO, "pread",          c_io_pread);
//  mrbc_define_method(vm, mrbc_class_IO, "pwrite",         c_io_pwrite);
//  mrbc_define_method(vm, mrbc_class_IO, "getbyte",        c_io_getbyte);
//  mrbc_define_method(vm, mrbc_class_IO, "readbyte",       c_io_readbyte);
}
