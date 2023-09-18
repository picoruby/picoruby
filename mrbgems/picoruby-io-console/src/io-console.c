#include <stdlib.h>

#include <mrubyc.h>

#include "./hal/hal.h"

#ifdef MRBC_USE_HAL_POSIX
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>

static struct termios save_settings;
static int save_flags;

static void
c_raw_bang(mrbc_vm *vm, mrbc_value *v, int argc)
{
  struct termios settings;
  tcgetattr(fileno(stdin), &save_settings);
  settings = save_settings;
  settings.c_iflag = ~(BRKINT | ISTRIP | IXON);
  settings.c_lflag = ~(ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHONL);
  settings.c_cc[VMIN]  = 1;
  settings.c_cc[VTIME] = 0;
  tcsetattr(fileno(stdin), TCSANOW, &settings);
  save_flags = fcntl(fileno(stdin), F_GETFL, 0);
  fcntl(fileno(stdin), F_SETFL, save_flags | O_NONBLOCK); /* add `non blocking` */
  SET_RETURN(v[0]);
}

static void
c_cooked_bang(mrbc_vm *vm, mrbc_value *v, int argc)
{
  fcntl(fileno(stdin), F_SETFL, save_flags);
  tcsetattr(fileno(stdin), TCSANOW, &save_settings);
  SET_RETURN(v[0]);
}

#else

void c_raw_bang(mrbc_vm *vm, mrbc_value *v, int argc);
void c_cooked_bang(mrbc_vm *vm, mrbc_value *v, int argc);

#endif /* MRBC_USE_HAL_POSIX */

static void
c_getc(mrbc_vm *vm, mrbc_value *v, int argc)
{
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
  c_raw_bang(vm, v, 0);
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
  c_cooked_bang(vm, v, 0);
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

void
mrbc_io_console_init(void)
{
  mrbc_class *class_IO = mrbc_define_class(0, "IO", mrbc_class_object);
  mrbc_define_method(0, class_IO, "read_nonblock", c_read_nonblock);
  mrbc_define_method(0, class_IO, "getc", c_getc);

#ifdef MRBC_USE_HAL_POSIX
  mrbc_define_method(0, class_IO, "raw!", c_raw_bang);
  mrbc_define_method(0, class_IO, "cooked!", c_cooked_bang);
#endif /* MRBC_USE_HAL_POSIX */
}
