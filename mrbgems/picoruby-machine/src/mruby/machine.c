#include "mruby.h"
#include "mruby/string.h"
#include "mruby/presym.h"
#include "mruby/array.h"


static mrb_value
mrb_s_tud_task(mrb_state *mrb, mrb_value klass)
{
  Machine_tud_task();
  return mrb_nil_value();
}

static mrb_value
mrb_s_tud_mounted_p(mrb_state *mrb, mrb_value klass)
{
  if (Machine_tud_mounted_q()) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_s_delay_ms(mrb_state *mrb, mrb_value klass)
{
  mrb_int ms;
  mrb_get_args(mrb, "i", &ms);
  if (ms < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "delay time must be positive");
  }
  Machine_delay_ms((uint32_t)ms);
  return mrb_fixnum_value(ms);
}

static mrb_value
mrb_s_busy_wait_ms(mrb_state *mrb, mrb_value klass)
{
  mrb_int ms;
  mrb_get_args(mrb, "i", &ms);
  if (ms < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "delay time must be positive");
  }
  Machine_busy_wait_ms(ms);
  return mrb_fixnum_value(ms);
}

static mrb_value
mrb_s_sleep(mrb_state *mrb, mrb_value klass)
{
  mrb_int sec;
  mrb_get_args(mrb, "i", &sec);
  if (sec < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "delay time must be positive");
  }
  if (24 * 60 * 60 <= sec) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "sleep length must be less than 24 hours (86400 sec)");
  } else if (sec < 2) {
    // Hangs if you attempt to sleep for 1 second.
    mrb_warn(mrb, "Cannot sleep less than 2 sec\n");
  } else {
    mrb_warn(mrb, "Going to sleep %d sec (USC-CDC will not be back)\n", sec);
    Machine_sleep(sec);
  }
  return mrb_fixnum_value(sec);
}

static mrb_value
mrb_s_deep_sleep(mrb_state *mrb, mrb_value klass)
{
  mrb_notimplement(mrb);
  // TODO: Implement
  mrb_int sec;
  mrb_get_args(mrb, "i", &sec);
  mrb_warn(mrb, "Going to deep sleep\n");
  //Machine_deep_sleep(GET_INT_ARG(1), v[2].tt == mrb_TT_TRUE, v[3].tt == mrb_TT_TRUE);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_s_unique_id(mrb_state *mrb, mrb_value klass)
{
  char id[33] = {0};
  if (Machine_get_unique_id(id)) {
    mrb_value ret = mrb_str_new_cstr(mrb, (const char *)id);
    return ret;
  } else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb_s_read_memory(mrb_state *mrb, mrb_value klass)
{
  mrb_int addr, size;
  mrb_get_args(mrb, "ii", &addr, &size);
  return mrb_str_new(mrb, (const void *)(uintptr_t)addr, size);
}

static mrb_value
mrb_s_stack_usage(mrb_state *mrb, mrb_value klass)
{
  mrb_int usage = Machine_stack_usage();
  if (0 < usage) {
    return mrb_fixnum_value(usage);
  } else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb_s_mcu_name(mrb_state *mrb, mrb_value klass)
{
  const char *name = Machine_mcu_name();
  return mrb_str_new_cstr(mrb, name);
}

#if !defined(PICORB_PLATFORM_POSIX)
#include <time.h>
#endif

static mrb_value
mrb_s_set_hwclock(mrb_state *mrb, mrb_value self)
{
#if defined(PICORB_PLATFORM_POSIX)
  mrb_notimplement(mrb);
  return mrb_nil_value();
#else
  mrb_int tv_sec, tv_nsec;
  mrb_get_args(mrb, "ii", &tv_sec, &tv_nsec);
  const struct timespec ts = {
    .tv_sec = (time_t)tv_sec,
    .tv_nsec = (long)tv_nsec,
  };
  if (Machine_set_hwclock(&ts)) {
    return mrb_true_value();
  } else {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to set hwclock");
  }
#endif
}

static mrb_value
mrb_s_get_hwclock(mrb_state *mrb, mrb_value self)
{
#if defined(PICORB_PLATFORM_POSIX)
  mrb_notimplement(mrb);
  return mrb_nil_value();
#else
  struct timespec ts = {0};
  if (Machine_get_hwclock(&ts)) {
    mrb_value ary = mrb_ary_new_capa(mrb, 2);
    mrb_ary_set(mrb, ary, 0, mrb_int_value(mrb, (mrb_int)ts.tv_sec));
    mrb_ary_set(mrb, ary, 1, mrb_int_value(mrb, (mrb_int)ts.tv_nsec));
    return ary;
  } else {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to get hwclock");
  }
#endif
}
#if !defined(PICORB_PLATFORM_POSIX)
static mrb_noreturn void
raise_interrupt(mrb_state *mrb)
{
  struct RClass *classInterrupt = mrb_class_get_id(mrb, MRB_SYM(Interrupt));
  mrb_raise(mrb, classInterrupt, "SIGINT");
}

static mrb_noreturn void
raise_sigtstp(mrb_state *mrb)
{
  struct RClass *class_SignalException = mrb_class_get_id(mrb, MRB_SYM(SignalException));
  mrb_raise(mrb, class_SignalException, "SIGTSTP");
}


static size_t
print_sub(mrb_state *mrb, mrb_value obj)
{
  mrb_value str = mrb_funcall(mrb, obj, "to_s", 0);
  const char *cstr = RSTRING_PTR(str);
  size_t len = RSTRING_LEN(str);
  hal_write(0, cstr, len);
  return len;
}

static mrb_value
mrb_io_puts(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*", &argv, &argc);
  if (argc == 0) {
    hal_write(0, "\n", 1);
  } else {
    int ai = mrb_gc_arena_save(mrb);
    for (mrb_int i = 0; i < argc; i++) {
      print_sub(mrb, argv[i]);
      hal_write(0, "\n", 1);
    }
    mrb_gc_arena_restore(mrb, ai);
  }
  return mrb_nil_value();
}

static mrb_value
mrb_io_print(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  mrb_int argc;
  int ai = mrb_gc_arena_save(mrb);
  mrb_get_args(mrb, "*", &argv, &argc);
  for (mrb_int i = 0; i < argc; i++) {
    print_sub(mrb, argv[i]);
  }
  mrb_gc_arena_restore(mrb, ai);
  return mrb_nil_value();
}

static mrb_value
mrb_io_write(mrb_state *mrb, mrb_value self)
{
  mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*", &argv, &argc);
  size_t total = 0;
  for (mrb_int i = 0; i < argc; i++) {
    total += print_sub(mrb, argv[i]);
  }
  return mrb_fixnum_value(total);
}

static mrb_value
mrb_io_gets(mrb_state *mrb, mrb_value self)
{
  mrb_value str = mrb_str_new(mrb, "", 0);
  char buf[2];
  buf[1] = '\0';
  while (true) {
    int c = hal_getchar();
    if (c == 3) { // Ctrl-C
      raise_interrupt(mrb); // mrb_noreturn
    } else if (c == 26) { // Ctrl-Z
      raise_sigtstp(mrb); // mrb_noreturn
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
  mrb_value str = mrb_io_gets(mrb, self);
  if (1 < RSTRING_LEN(str)) {
    mrb_str_resize(mrb, str, 1);
  }
  return str;
}
#endif


static mrb_value
mrb_s_exit(mrb_state *mrb, mrb_value self)
{
  mrb_int status = 0;
  mrb_get_args(mrb, "|i", &status);
  Machine_exit(status);
  return mrb_nil_value(); // This line will never be reached.
}

void
mrb_picoruby_machine_gem_init(mrb_state* mrb)
{
  mrb_define_class_id(mrb, MRB_SYM(IOError), E_STANDARD_ERROR);

  struct RClass *class_Machine = mrb_define_class_id(mrb, MRB_SYM(Machine), mrb->object_class);

  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(tud_task), mrb_s_tud_task, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM_Q(tud_mounted), mrb_s_tud_mounted_p, MRB_ARGS_NONE());

  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(delay_ms), mrb_s_delay_ms, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(busy_wait_ms), mrb_s_busy_wait_ms, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(sleep), mrb_s_sleep, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(deep_sleep), mrb_s_deep_sleep, MRB_ARGS_REQ(3));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(unique_id), mrb_s_unique_id, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(read_memory), mrb_s_read_memory, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(stack_usage), mrb_s_stack_usage, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(mcu_name), mrb_s_mcu_name, MRB_ARGS_NONE());

  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(set_hwclock), mrb_s_set_hwclock, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(get_hwclock), mrb_s_get_hwclock, MRB_ARGS_NONE());

  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(exit), mrb_s_exit, MRB_ARGS_OPT(1));

#if !defined(PICORB_PLATFORM_POSIX)
  struct RClass *class_IO = mrb_define_class_id(mrb, MRB_SYM(IO), mrb->object_class);
  mrb_define_method_id(mrb, class_IO, MRB_SYM(puts), mrb_io_puts, MRB_ARGS_ANY());
  mrb_define_method_id(mrb, class_IO, MRB_SYM(print), mrb_io_print, MRB_ARGS_ANY());
  mrb_define_method_id(mrb, class_IO, MRB_SYM(write), mrb_io_write, MRB_ARGS_ANY());
  mrb_define_method_id(mrb, class_IO, MRB_SYM(gets), mrb_io_gets, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_IO, MRB_SYM(getc), mrb_io_getc, MRB_ARGS_NONE());
#endif
}

void
mrb_picoruby_machine_gem_final(mrb_state* mrb)
{
}
