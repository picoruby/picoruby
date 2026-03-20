/*
** file_ext.c - Extensions to mruby-io File class
**
** Adds File#fsync for POSIX platforms.
*/

#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/class.h>
#include "../lib/mruby/mrbgems/mruby-io/include/mruby/io.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#endif

/*
 * call-seq:
 *   file.fsync -> 0
 *
 * Flushes file data and metadata to disk.
 * Raises IOError on failure.
 */
static mrb_value
mrb_file_fsync(mrb_state *mrb, mrb_value self)
{
  int fd = mrb_io_fileno(mrb, self);

#if defined(_WIN32) || defined(_WIN64)
  if (_commit(fd) < 0) {
#else
  if (fsync(fd) < 0) {
#endif
    mrb_raisef(mrb, E_IO_ERROR, "fsync failed: %s", strerror(errno));
  }

  return mrb_fixnum_value(0);
}

void
mrb_file_ext_init(mrb_state *mrb)
{
  struct RClass *file = mrb_class_get_id(mrb, MRB_SYM(File));
  mrb_define_method_id(mrb, file, MRB_SYM(fsync), mrb_file_fsync, MRB_ARGS_NONE());
}
