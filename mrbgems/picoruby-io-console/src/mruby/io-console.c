#include <mruby.h>
#include <mruby/string.h>
#include <mruby/presym.h>

#include <string.h>

static mrb_value
mrb_io_read_nonblock(mrb_state *mrb, mrb_value self)
{
  /*
    * read_nonblock(maxlen, outbuf = nil, exeption: true) -> String | Symbol | nil
    * only supports `exception: false` mode
    */
  mrb_int maxlen;
  mrb_value outbuf;
  mrb_get_args(mrb, "i|S", &maxlen, &outbuf);
  char buf[maxlen + 1];
  mrb_int len;
  io_raw_bang(true);
  int c;
  for (len = 0; len < maxlen; len++) {
    c = hal_getchar();
    if (c < 0) {
      break;
    } else {
      buf[len] = c;
    }
  }
  buf[len] = '\0';
  io__restore_termios();
  if (1 < mrb_get_argc(mrb)) {
    outbuf = mrb_str_resize(mrb, outbuf, maxlen);
    memcpy(RSTRING_PTR(outbuf), buf, len);
    return outbuf;
  } else if (len < 1) {
    return mrb_nil_value();
  } else {
    outbuf = mrb_str_new_cstr(mrb, buf);
    return outbuf;
  }
}

static mrb_value
mrb_io_raw_b(mrb_state *mrb, mrb_value self)
{
  io_raw_bang(false);
  return self;
}

static mrb_value
mrb_io_cooked_b(mrb_state *mrb, mrb_value self)
{
  io_cooked_bang();
  return self;
}

static mrb_value
mrb_io__restore_termios(mrb_state *mrb, mrb_value self)
{
  io__restore_termios();
  return self;
}

static mrb_value
mrb_io_echo_e(mrb_state *mrb, mrb_value self)
{
  mrb_bool arg;
  mrb_get_args(mrb, "b", &arg);
  io_echo_eq(arg);
  return arg ? mrb_true_value() : mrb_false_value();
}

static mrb_value
mrb_io_echo_q(mrb_state *mrb, mrb_value self)
{
  if (io_echo_q()) {
    return mrb_true_value();
  }
  else {
    return mrb_false_value();
  }
}

#if !defined(PICORB_PLATFORM_POSIX)
static mrb_value
mrb_io_gets(mrb_state *mrb, mrb_value self)
{
  mrb_value str = mrb_str_new(mrb, "", 0);
  char buf[2];
  buf[1] = '\0';
  while (true) {
    int c = hal_getchar();
    if (c == 3) { // Ctrl-C
      mrb_raise(mrb, mrb_class_get_id(mrb, MRB_SYM(IOError)), "Interrupt");
    } if (c == 27) { // ESC continue;
    }
    if (c == 8 || c == 127) { // Backspace
      if (0 < RSTRING_LEN(str)) { mrb_str_resize(mrb, str, RSTRING_LEN(str) - 1); hal_write(1, "\b \b", 3);
      }
    } else
    if (-1 < c) {
      buf[0] = c;
      mrb_str_cat(mrb, str, (const char *)buf, 1);
      hal_write(1, buf, 1);
      if (c == '\n' || c == '\r') {
        break;
      }
    }
  }
  return str;
}

static mrb_value
mrb_io_getc(mrb_state *mrb, mrb_value self)
{
  if (io_raw_q()) {
    char buf[1];
    int c = hal_getchar();
    if (-1 < c) {
      buf[0] = c;
      return mrb_str_new(mrb, buf, 1);
    } else {
      return mrb_nil_value();
    }
  }
  else {
    mrb_value str = mrb_io_gets(mrb, self);
    if (1 < RSTRING_LEN(str)) {
      mrb_str_resize(mrb, str, 1);
    }
    return str;
  }
}
#endif


void
mrb_picoruby_io_console_gem_init(mrb_state* mrb)
{
  struct RClass *class_IO = mrb_class_get(mrb, "IO");

  mrb_define_method_id(mrb, class_IO, MRB_SYM(read_nonblock), mrb_io_read_nonblock, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, class_IO, MRB_SYM_B(raw), mrb_io_raw_b, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_IO, MRB_SYM_B(cooked), mrb_io_cooked_b, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_IO, MRB_SYM(_restore_termios), mrb_io__restore_termios, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_IO, MRB_SYM_E(echo), mrb_io_echo_e, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_IO, MRB_SYM_Q(echo), mrb_io_echo_q, MRB_ARGS_NONE());
#if !defined(PICORB_PLATFORM_POSIX)
  mrb_define_method_id(mrb, class_IO, MRB_SYM(gets), mrb_io_gets, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_IO, MRB_SYM(getc), mrb_io_getc, MRB_ARGS_NONE());
#endif
}

void
mrb_picoruby_io_console_gem_final(mrb_state* mrb)
{
}
