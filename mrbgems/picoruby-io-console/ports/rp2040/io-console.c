#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <mrubyc.h>

/*-------------------------------------
 *
 *  IO::Console
 *
 *------------------------------------*/

static bool raw_mode = false;
static bool raw_mode_saved = false;
static bool echo_mode = true;
static bool echo_mode_saved = true;

void
c_raw_bang(mrb_vm *vm, mrb_value *v, int argc)
{
  raw_mode_saved = raw_mode;
  raw_mode = true;
}

void
c_cooked_bang(mrb_vm *vm, mrb_value *v, int argc)
{
  raw_mode_saved = raw_mode;
  raw_mode = false;
}

static void
c_echo_eq(mrb_vm *vm, mrb_value *v, int argc)
{
  echo_mode_saved = echo_mode;
  if (v[1].tt == MRBC_TT_FALSE) {
    echo_mode = false;
  } else {
    echo_mode = true;
  }
}

static void
c_echo_q(mrb_vm *vm, mrb_value *v, int argc)
{
  if (echo_mode) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
c__restore_termios(mrb_vm *vm, mrb_value *v, int argc)
{
  raw_mode = raw_mode_saved;
  echo_mode = echo_mode_saved;
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
      mrbc_raise(vm, MRBC_CLASS(IOError), "Interrupted");
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
  if (raw_mode) {
    char buf[1];
    int c = hal_getchar();
    if (-1 < c) {
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

void
io_console_port_init(mrbc_vm *vm, mrbc_class *class_IO)
{
  mrbc_define_method(vm, class_IO, "raw!", c_raw_bang);
  mrbc_define_method(vm, class_IO, "cooked!", c_cooked_bang);
  mrbc_define_method(vm, class_IO, "echo?", c_echo_q);
  mrbc_define_method(vm, class_IO, "echo=", c_echo_eq);
  mrbc_define_method(vm, class_IO, "_restore_termios", c__restore_termios);
  mrbc_define_method(vm, class_IO, "getc", c_getc);
  mrbc_define_method(vm, mrbc_class_object, "gets", c_gets);
}

