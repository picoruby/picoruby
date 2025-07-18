#include <mrubyc.h>

static void
raise_interrupt(mrbc_vm *vm)
{
  mrbc_class *interrupt = mrbc_get_class_by_name("Interrupt");
  mrbc_raise(vm, interrupt, "Interrupted");
}

static void
c_raw_bang(mrbc_vm *vm, mrbc_value *v, int argc)
{
  io_raw_bang(false);
}

static void
c_cooked_bang(mrbc_vm *vm, mrbc_value *v, int argc)
{
  io_cooked_bang();
}

static void
c__restore_termios(mrbc_vm *vm, mrbc_value *v, int argc)
{
  io__restore_termios();
}

static void
c_echo_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  io_echo_eq(v[1].tt == MRBC_TT_TRUE);
  SET_RETURN(v[1]);

}

static void
c_echo_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (io_echo_q()) {
    SET_TRUE_RETURN();
  }
  else {
    SET_FALSE_RETURN();
  }
}

static void
c_read_nonblock(mrbc_vm *vm, mrbc_value *v, int argc)
{
  /*
    * read_nonblock(maxlen, outbuf = nil, exeption: true) -> String | Symbol | nil
    * only supports `exception: false` mode
    */
  int maxlen = GET_INT_ARG(1);
  char buf[maxlen + 1];
  mrbc_value outbuf;
  int len;
  io_raw_bang(true); // TODO fix this
  int c;
  for (len = 0; len < maxlen; len++) {
    c = hal_getchar();
    if (c == 3) {
      raise_interrupt(vm);
      return;
    } else if (c < 0) {
      break;
    } else {
      buf[len] = c;
    }
  }
  buf[len] = '\0';
  io__restore_termios();
  if (GET_ARG(2).tt == MRBC_TT_STRING) {
    outbuf = GET_ARG(2);
    uint8_t *str = outbuf.string->data;
    int orig_len = outbuf.string->size;
    if (orig_len != len) {
      str = mrbc_realloc(vm, str, len + 1);
      if (!str) {
        mrbc_raise(vm, MRBC_CLASS(RuntimeError), "no memory");
        return;
      }
    }
    memcpy(str, buf, len);
    str[len] = '\0';
    outbuf.string->data = str;
    outbuf.string->size = len;
    SET_RETURN(outbuf);
  } else if (len < 1) {
    SET_NIL_RETURN();
  } else {
    outbuf = mrbc_string_new_cstr(vm, buf);
    SET_RETURN(outbuf);
  }
}

#if !defined(PICORB_PLATFORM_POSIX)
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
  if (io_raw_q()) {
    char buf[1];
    int c = hal_getchar();
    if (c == 3) { // Ctrl-C
      raise_interrupt(vm);
      return;
    } else if (-1 < c) {
      buf[0] = c;
      mrb_value str = mrbc_string_new(vm, buf, 1);
      SET_RETURN(str);
    } else {
      SET_NIL_RETURN();
    }
  }
  else {
    c_gets(vm, v, argc);
    mrbc_value str = v[0];
    if (1 < str.string->size) {
      mrbc_realloc(vm, str.string->data, 1);
      str.string->size = 1;
    }
  }
}

#endif


void
mrbc_io_console_init(mrbc_vm *vm)
{
  mrbc_class *class_IO = mrbc_define_class(vm, "IO", mrbc_class_object);
  mrbc_define_method(vm, class_IO, "read_nonblock", c_read_nonblock);
  mrbc_define_method(vm, class_IO, "raw!", c_raw_bang);
  mrbc_define_method(vm, class_IO, "cooked!", c_cooked_bang);
  mrbc_define_method(vm, class_IO, "_restore_termios", c__restore_termios);
  mrbc_define_method(vm, class_IO, "echo=", c_echo_eq);
  mrbc_define_method(vm, class_IO, "echo?", c_echo_q);
#if !defined(PICORB_PLATFORM_POSIX)
  mrbc_define_method(vm, class_IO, "gets", c_gets);
  mrbc_define_method(vm, class_IO, "getc", c_getc);
#endif
}
