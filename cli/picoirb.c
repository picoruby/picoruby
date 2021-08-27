#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <errno.h>

#include "../src/mrubyc/src/mrubyc.h"

#if defined(PICORBC_DEBUG) && !defined(MRBC_ALLOC_LIBC)
  #include "../src/mrubyc/src/alloc.c"
#endif

#include "../src/picorbc.h"

#include "heap.h"

/* Ruby */
#include "ruby/buffer.c"
#include "ruby/irb.c"

#include "sandbox.h"

int sigint;
int loglevel;
static uint8_t heap[HEAP_SIZE];

void
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

void
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

#include <signal.h>

void
signal_handler(int _no)
{
  sigint = 1;
}

void
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

void
default_sigint(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = SIG_DFL;
  if( sigaction( SIGINT, &sa, NULL ) < 0 ) {
    perror("sigaction");
  }
}

void
c_terminate_sandbox(mrb_vm *vm, mrb_value *v, int argc)
{
  default_sigint();
  raise(SIGINT);
}

int
main(int argc, char *argv[])
{
  loglevel = LOGLEVEL_ERROR;
  mrbc_init(heap, HEAP_SIZE);
  mrbc_define_method(0, mrbc_class_object, "getch", c_getch);
  mrbc_define_method(0, mrbc_class_object, "gets_nonblock", c_gets_nonblock);
  mrbc_define_method(0, mrbc_class_object, "terminate_sandbox", c_terminate_sandbox);
  SANDBOX_INIT();
  create_sandbox();
  mrbc_create_task(buffer, 0);
  mrbc_create_task(irb, 0);
  ignore_sigint();
  mrbc_run();
  return 0;
}
