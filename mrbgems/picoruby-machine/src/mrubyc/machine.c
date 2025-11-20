#include "mrubyc.h"


static void
c_Machine_tud_task(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Machine_tud_task();
  SET_NIL_RETURN();
}

static void
c_Machine_tud_mounted_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (Machine_tud_mounted_q()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_Machine_delay_ms(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_FIXNUM) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
    return;
  }
  uint32_t ms = GET_INT_ARG(1);
  Machine_delay_ms(ms);
  SET_INT_RETURN(ms);
}

static void
c_Machine_busy_wait_ms(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_FIXNUM) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
    return;
  }
  uint32_t ms = GET_INT_ARG(1);
  Machine_busy_wait_ms(ms);
  SET_INT_RETURN(ms);
}

static void
c_Machine_sleep(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_FIXNUM) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
    return;
  }
  uint32_t sec = GET_INT_ARG(1);
  if (24 * 60 * 60 <= sec) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "sleep length must be less than 24 hours (86400 sec)");
    return;
  } else if (sec < 2) {
    // Hangs if you attempt to sleep for 1 second.
    console_printf("Cannot sleep less than 2 sec\n");
  } else {
    console_printf("Going to sleep %d sec\n", sec);
    Machine_sleep(sec);
  }
  SET_INT_RETURN(sec);
}

static void
c_Machine_deep_sleep(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "Not implemented");
  return;
  // TODO: Implement
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_FIXNUM) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
    return;
  }
  console_printf("Going to deep sleep\n");
  Machine_deep_sleep(GET_INT_ARG(1), v[2].tt == MRBC_TT_TRUE, v[3].tt == MRBC_TT_TRUE);
  SET_INT_RETURN(0);
}

static void
c_Machine_unique_id(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  char id[33] = {0};
  if (Machine_get_unique_id(id)) {
    mrbc_value ret = mrbc_string_new_cstr(vm, (const char *)id);
    SET_RETURN(ret);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_Machine_read_memory(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const void *addr = (const void *)(uintptr_t)GET_INT_ARG(1);
  uint32_t size = GET_INT_ARG(2);
  mrbc_value ret = mrbc_string_new(vm, addr, size);
  SET_RETURN(ret);
}

static void
c_Machine_stack_usage(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_int_t usage = Machine_stack_usage();
  if (0 < usage) {
    SET_INT_RETURN(usage);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_Machine_mcu_name(mrbc_vm *vm, mrbc_value *v, int argc)
{
  const char *name = Machine_mcu_name();
  mrbc_value ret = mrbc_string_new_cstr(vm, name);
  SET_RETURN(ret);
}

#if !defined(PICORB_PLATFORM_POSIX)
#include <time.h>
#endif

static void
c_Machine_set_hwclock(mrbc_vm *vm, mrbc_value *v, int argc)
{
#if defined(PICORB_PLATFORM_POSIX)
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "Not implemented");
#else
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
    return;
  }
  const struct timespec ts = {
    .tv_sec = (time_t)GET_INT_ARG(1),
    .tv_nsec = 0
  };
  if (Machine_set_hwclock(&ts)) {
    SET_TRUE_RETURN();
  } else {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Failed to set hwclock");
  }
#endif
}

static void
c_Machine_get_hwclock(mrbc_vm *vm, mrbc_value *v, int argc)
{
#if defined(PICORB_PLATFORM_POSIX)
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "Not implemented");
#else
  struct timespec ts = {0};
  if (Machine_get_hwclock(&ts)) {
    mrbc_value ary = mrbc_array_new(vm, 2);
    mrbc_array_set(&ary, 0, &mrbc_integer_value((mrbc_int_t)ts.tv_sec));
    mrbc_array_set(&ary, 1, &mrbc_integer_value((mrbc_int_t)ts.tv_nsec));
    SET_RETURN(ary);
  } else {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Failed to get hwclock");
  }
#endif
}

static void
c_Machine_uptime_us(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN((mrbc_uint_t)Machine_uptime_us());
}

static void
c_Machine_uptime_formatted(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char buf[20] = {0};
  Machine_uptime_formatted(buf, sizeof(buf));
  mrbc_value ret = mrbc_string_new_cstr(vm, buf);
  SET_RETURN(ret);
}

static void
c_Machine_debug_puts(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc == 0) {
    hal_write(2, "\n", 1);
  } else {
    for (int i = 1; i <= argc; i++) {
      mrbc_value arg = GET_ARG(i);
      mrbc_value str = mrbc_send(vm, v, argc, &arg, "to_s", 0);
      if (str.tt == MRBC_TT_STRING) {
        hal_write(2, (const char *)str.string->data, str.string->size);
        hal_write(2, "\n", 1);
      }
    }
  }
}

static void
c_Machine_exit(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int status;
  if (argc == 0) {
    status = 0;
  } else if (argc == 1) {
    if (GET_TT_ARG(1) != MRBC_TT_INTEGER) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
      return;
    }
    status = GET_INT_ARG(1);
  } else {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  Machine_exit(status);
  SET_NIL_RETURN();
}

#if !defined(PICORB_PLATFORM_POSIX)
static void
raise_interrupt(mrbc_vm *vm)
{
  mrbc_class *interrupt = mrbc_get_class_by_name("Interrupt");
  mrbc_raise(vm, interrupt, "Interrupted");
}

static void
c_gets(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrb_value str = mrbc_string_new(vm, NULL, 0);
  char buf[2];
  buf[1] = '\0';
  while (true) {
    int c = hal_getchar();
    if (c == 3) { // Ctrl-C
      raise_interrupt(vm);
      return;
    }
    if (c == 27) { // ESC
      continue;
    }
    if (c == 8 || c == 127) { // Backspace
      if (0 < str.string->size) {
        str.string->size--;
        mrbc_realloc(vm, str.string->data, str.string->size);
        hal_write(1, "\b \b", 3);
      }
    } else
    if (-1 < c) {
      buf[0] = c;
      mrbc_string_append_cstr(&str, buf);
      hal_write(1, buf, 1);
      if (c == '\n' || c == '\r') {
        break;
      }
    }
  }
  SET_RETURN(str);
}

static void
c_getc(mrbc_vm *vm, mrbc_value *v, int argc)
{
  c_gets(vm, v, argc);
  mrbc_value str = v[0];
  if (1 < str.string->size) {
    mrbc_realloc(vm, str.string->data, 1);
    str.string->size = 1;
  }
}

static void
c_io_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mrbc_value *arg = &v[1];
  const char *str;
  int len;

  if (arg->tt == MRBC_TT_STRING) {
    str = (const char *)arg->string->data;
    len = arg->string->size;
  } else {
    // For non-string, just use mrbc_print_sub which handles conversion
    // This is simpler and avoids calling to_s which might be implemented in Ruby
    SET_INT_RETURN(0);
    return;
  }

  // Get the fd from the IO instance
  // Assume fd is stored in @fd instance variable
  mrbc_value fd_val = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("fd"));
  int fd = 1; // default to stdout
  if (fd_val.tt == MRBC_TT_FIXNUM) {
    fd = fd_val.i;
  }

  int written = hal_write(fd, str, len);
  SET_INT_RETURN(written);
}
#endif


void
mrbc_machine_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_Machine = mrbc_define_class(vm, "Machine", mrbc_class_object);

  mrbc_define_method(vm, mrbc_class_Machine, "tud_task", c_Machine_tud_task);
  mrbc_define_method(vm, mrbc_class_Machine, "tud_mounted?", c_Machine_tud_mounted_q);

  mrbc_define_method(vm, mrbc_class_Machine, "delay_ms", c_Machine_delay_ms);
  mrbc_define_method(vm, mrbc_class_Machine, "busy_wait_ms", c_Machine_busy_wait_ms);
  mrbc_define_method(vm, mrbc_class_Machine, "sleep", c_Machine_sleep);
  mrbc_define_method(vm, mrbc_class_Machine, "deep_sleep", c_Machine_deep_sleep);
  mrbc_define_method(vm, mrbc_class_Machine, "unique_id", c_Machine_unique_id);
  mrbc_define_method(vm, mrbc_class_Machine, "read_memory", c_Machine_read_memory);
  mrbc_define_method(vm, mrbc_class_Machine, "stack_usage", c_Machine_stack_usage);
  mrbc_define_method(vm, mrbc_class_Machine, "mcu_name", c_Machine_mcu_name);

  mrbc_define_method(vm, mrbc_class_Machine, "set_hwclock", c_Machine_set_hwclock);
  mrbc_define_method(vm, mrbc_class_Machine, "get_hwclock", c_Machine_get_hwclock);
  mrbc_define_method(vm, mrbc_class_Machine, "uptime_us", c_Machine_uptime_us);
  mrbc_define_method(vm, mrbc_class_Machine, "uptime_formatted", c_Machine_uptime_formatted);

  mrbc_define_method(vm, mrbc_class_Machine, "exit", c_Machine_exit);
  mrbc_define_method(vm, mrbc_class_Machine, "debug_puts", c_Machine_debug_puts);

#if !defined(PICORB_PLATFORM_POSIX)
  mrbc_class *class_IO = mrbc_define_class(vm, "IO", mrbc_class_object);
  mrbc_define_method(vm, class_IO, "gets", c_gets);
  mrbc_define_method(vm, class_IO, "getc", c_getc);
  mrbc_define_method(vm, class_IO, "write", c_io_write);
  /*
   * puts, print are implemented in mrblib/io.rb using write method
   * p is implemented in mrblib/kernel.rb
   */
#endif
}
