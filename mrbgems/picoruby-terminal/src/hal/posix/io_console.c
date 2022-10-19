#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <fcntl.h>

#include <mrubyc.h>
#include <picorbc.h>

#include "io_console.h"

static int sigint;
int loglevel = LOGLEVEL_FATAL;

static void
signal_handler(int _no)
{
  sigint = 1;
}

static void
ignore_sigint(void)
{
  sigint = 0;
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;
  if( sigaction( SIGINT, &sa, NULL ) < 0 ) {
    perror("sigaction");
  }
}

static void
default_sigint(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = SIG_DFL;
  if( sigaction( SIGINT, &sa, NULL ) < 0 ) {
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

static void
c_getch(mrb_vm *vm, mrb_value *v, int argc)
{
  struct termios save_settings;
  struct termios settings;
  tcgetattr( fileno( stdin ), &save_settings );
  settings = save_settings;
  settings.c_lflag &= ~( ECHO | ICANON ); /* no echoback & no wait for LF */
  tcsetattr( fileno( stdin ), TCSANOW, &settings );
  fcntl( fileno( stdin ), F_SETFL, O_NONBLOCK ); /* non blocking */
  int c;
  for (;;) {
    c = getchar();
    if (c != EOF) break;
    if (sigint) {
      c = 3;
      sigint = 0;
      break;
    }
  }
  SET_INT_RETURN(c);
  tcsetattr( fileno( stdin ), TCSANOW, &save_settings );
}

static void
c_gets_nonblock(mrb_vm *vm, mrb_value *v, int argc)
{
  size_t max_len = GET_INT_ARG(1) + 1;
  char buf[max_len];
  struct termios save_settings;
  struct termios settings;
  tcgetattr( fileno( stdin ), &save_settings );
  settings = save_settings;
  settings.c_lflag &= ~( ECHO | ICANON ); /* no echoback & no wait for LF */
  tcsetattr( fileno( stdin ), TCSANOW, &settings );
  fcntl( fileno( stdin ), F_SETFL, O_NONBLOCK ); /* non blocking */
  int c;
  size_t len;
  for(len = 0; len < max_len; len++) {
    c = getchar();
    if ( c == EOF ) {
      break;
    } else {
      buf[len] = c;
    }
  }
  buf[len] = '\0';
  tcsetattr( fileno( stdin ), TCSANOW, &save_settings );
  mrb_value value = mrbc_string_new(vm, (const void *)&buf, len);
  SET_RETURN(value);
}

void
mrbc_io_console_init(void)
{
  mrbc_define_method(0, mrbc_class_object, "gets_nonblock", c_gets_nonblock);
  mrbc_define_method(0, mrbc_class_object, "getch", c_getch);
  mrbc_define_method(0, mrbc_class_object, "terminate_irb", c_terminate_irb);
  mrbc_define_method(0, mrbc_class_object, "get_cursor_position", c_get_cursor_position);
  ignore_sigint();
}

