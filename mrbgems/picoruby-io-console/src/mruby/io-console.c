#include <mruby.h>
#include <mruby/string.h>
#include <mruby/presym.h>

#include <string.h>

// for POSIX
static mrb_noreturn void
raise_interrupt(mrb_state *mrb)
{
  struct RClass *class_Interrupt = mrb_class_get_id(mrb, MRB_SYM(Interrupt));
  mrb_raise(mrb, class_Interrupt, "SIGINT");
}

// for POSIX
static mrb_noreturn void
raise_sigtstp(mrb_state *mrb)
{
  struct RClass *class_SignalException = mrb_class_get_id(mrb, MRB_SYM(SignalException));
  mrb_raise(mrb, class_SignalException, "SIGTSTP");
}

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
    if (c == 3) {
      raise_interrupt(mrb); // mrb_noreturn
    } else if (c == 26) {
      raise_sigtstp(mrb); // mrb_noreturn
    } else if (c < 0) {
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


void
mrb_picoruby_io_console_gem_init(mrb_state* mrb)
{
  struct RClass *class_IO = mrb_define_class_id(mrb, MRB_SYM(IO), mrb->object_class);

  mrb_define_method_id(mrb, class_IO, MRB_SYM(read_nonblock), mrb_io_read_nonblock, MRB_ARGS_ARG(1, 1));
  mrb_define_method_id(mrb, class_IO, MRB_SYM_B(raw), mrb_io_raw_b, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_IO, MRB_SYM_B(cooked), mrb_io_cooked_b, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_IO, MRB_SYM(_restore_termios), mrb_io__restore_termios, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_IO, MRB_SYM_E(echo), mrb_io_echo_e, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_IO, MRB_SYM_Q(echo), mrb_io_echo_q, MRB_ARGS_NONE());
}

void
mrb_picoruby_io_console_gem_final(mrb_state* mrb)
{
}
