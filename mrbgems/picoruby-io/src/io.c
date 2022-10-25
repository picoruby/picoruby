#include <stdlib.h>

#include <mrubyc.h>

#include "./hal/hal.h"

static void
c_getch(mrb_vm *vm, mrb_value *v, int argc)
{
  int ch = hal_read_char(STDIN_FD);
  char buf[2];
  buf[0] = ch;
  buf[1] = '\0';
  mrbc_value str = mrbc_string_new_cstr(vm, buf);
  SET_RETURN(str);
}

static void
c_read_nonblock(mrb_vm *vm, mrb_value *v, int argc)
{
  /* 
   * read_nonblock(maxlen, outbuf = nil, exeption: true) -> String | Symbol | nil
   * TODO: 3rd parameter when karg implemented in PicoRuby compiler
   */
  int maxlen = GET_INT_ARG(1);
  if (!hal_read_available(STDIN_FD)) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "EWOULDBLOCK");
  } else {
    char buf[maxlen];
    mrbc_value outbuf;
    int len = hal_read_nonblock(STDIN_FD, buf, maxlen);
    if (GET_ARG(2).tt == MRBC_TT_STRING) {
      outbuf = GET_ARG(2);
      uint8_t *str = outbuf.string->data;
      int orig_len = outbuf.string->size;
      if (orig_len != len) {
        str = mrbc_realloc(vm, str, len + 1);
        if (!str) return;
      }
      memcpy(str, buf, len);
      str[len] = '\0';
      outbuf.string->data = str;
      outbuf.string->size = len;
    } else {
      outbuf = mrbc_string_new_cstr(vm, buf);
    }
    if (len < 1) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "EOFError");
    } else {
      SET_RETURN(outbuf);
    }
  }
}

#ifdef MRBC_USE_HAL_POSIX

#include <stdio.h>
#include <termios.h>
#include <signal.h>

static void
signal_handler(int _no)
{
//  sigint = 1;
  setGlobalSigint(1);
}

static void
ignore_sigint(void)
{
  //sigint = 0;
  setGlobalSigint(0);
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  if(sigaction(SIGINT, &sa, NULL) < 0) {
    perror("sigaction");
  }
}

static void
default_sigint(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = SIG_DFL;
  if(sigaction(SIGINT, &sa, NULL) < 0) {
    perror("sigaction");
  }
}

static void
c_terminate_irb(mrb_vm *vm, mrb_value *v, int argc)
{
  default_sigint();
  raise(SIGINT);
}

static void
c_get_cursor_position(struct VM *vm, mrbc_value v[], int argc)
{
  static struct termios state, oldstate;

  char buf[10];
  char *p1 = buf;
  char *p2 = NULL;
  char c;

  int row = 0;
  int col = 0;
  int res;

  // echo off
  tcgetattr(0, &oldstate);
  state = oldstate;
  state.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(0, TCSANOW, &state);

  // report cursor position
  res = write(0, "\e[6n", 4);
  if (res != 4) mrbc_raise(vm, MRBC_CLASS(RuntimeError), "write() failed");

  for (;;) {
    res = read(0, &c, 1);
    // if (res < 0) ignore
    if (res == 0) break;
    if(0x30 <= c && c <= 0x39) *p1++ = c;
    if(c == ';') {
      *p1++ = '\0';
      row = atoi(buf);
      p2 = p1;
    }
    if(c == 'R') break;
  }
  *p1 = '\0';
  col = atoi(p2);

  // echo on
  tcsetattr(0, TCSANOW, &oldstate);

  mrbc_value val_array = mrbc_array_new(vm, 2);
  val_array.array->n_stored = 2;
  if (row && col) {
    mrbc_set_integer(val_array.array->data, row);
    mrbc_set_integer(val_array.array->data + 1, col);
  } else {
    mrbc_raise(vm, MRBC_CLASS(Exception), "get_cursor_position failed");
  }
  SET_RETURN(val_array);
}
#endif /* MRBC_USE_HAL_POSIX */

void
mrbc_io_init(void)
{
  mrbc_class *class_IO = mrbc_define_class(0, "IO", mrbc_class_object);
  mrbc_define_method(0, class_IO, "read_nonblock", c_read_nonblock);
  mrbc_define_method(0, class_IO, "getch", c_getch);

#ifdef MRBC_USE_HAL_POSIX
  mrbc_define_method(0, mrbc_class_object, "terminate_irb", c_terminate_irb);
  mrbc_define_method(0, class_IO, "get_cursor_position", c_get_cursor_position);
  ignore_sigint();
#endif /* MRBC_USE_HAL_POSIX */
}
