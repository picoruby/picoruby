#ifndef IO_DEFINED_H_
#define IO_DEFINED_H_

#include <mrubyc.h>

#ifdef PICORB_NO_STDIO
# error IO and File conflicts 'PICORB_NO_STDIO' in your build configuration
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(PICORB_WITHOUT_IO_PREAD_PWRITE)
# undef PICORB_WITH_IO_PREAD_PWRITE
#elif !defined(PICORB_WITH_IO_PREAD_PWRITE)
# if defined(__unix__) || defined(__MACH__)
#  define PICORB_WITH_IO_PREAD_PWRITE
# endif
#endif

#define PICORB_IO_BUF_SIZE 4096

struct picorb_io_buf {
  short start;
  short len;
  char mem[PICORB_IO_BUF_SIZE];
};

struct picorb_io {
  unsigned int readable:1,
               writable:1,
               eof:1,
               sync:1,
               is_socket:1,
               close_fd:1,
               close_fd2:1;
  int fd;   /* file descriptor, or -1 */
  int fd2;  /* file descriptor to write if it's different from fd, or -1 */
  int pid;  /* child's pid (for pipes)  */
  struct picorb_io_buf *buf;
};

#define PICORB_O_RDONLY            0x0000
#define PICORB_O_WRONLY            0x0001
#define PICORB_O_RDWR              0x0002
#define PICORB_O_ACCMODE           (PICORB_O_RDONLY | PICORB_O_WRONLY | PICORB_O_RDWR)
#define PICORB_O_NONBLOCK          0x0004
#define PICORB_O_APPEND            0x0008
#define PICORB_O_SYNC              0x0010
#define PICORB_O_NOFOLLOW          0x0020
#define PICORB_O_CREAT             0x0040
#define PICORB_O_TRUNC             0x0080
#define PICORB_O_EXCL              0x0100
#define PICORB_O_NOCTTY            0x0200
#define PICORB_O_DIRECT            0x0400
#define PICORB_O_BINARY            0x0800
#define PICORB_O_SHARE_DELETE      0x1000
#define PICORB_O_TMPFILE           0x2000
#define PICORB_O_NOATIME           0x4000
#define PICORB_O_DSYNC             0x00008000
#define PICORB_O_RSYNC             0x00010000

//#define E_IO_ERROR              mrb_exc_get_id(mrb, MRBC_SYM(IOError))
//#define E_EOF_ERROR             mrb_exc_get_id(mrb, MRBC_SYM(EOFError))

int picorb_io_fileno(mrbc_vm *vm, mrbc_value io);
void mrbc_io_file_init(mrbc_vm *vm, mrbc_class *io);

#if defined(__cplusplus)
} /* extern "C" { */
#endif
#endif /* IO_DEFINED_H_ */
