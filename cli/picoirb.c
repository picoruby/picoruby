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

int
main(int argc, char *argv[])
{
  loglevel = LOGLEVEL_WARN;
  mrbc_init(heap, HEAP_SIZE);
  mrbc_define_method(0, mrbc_class_object, "getch", c_getch);
  mrbc_define_method(0, mrbc_class_object, "gets_nonblock", c_gets_nonblock);
  SANDBOX_INIT();
  create_sandbox();
  mrbc_create_task(buffer, 0);
  mrbc_create_task(irb, 0);
  mrbc_run();
  return 0;
}
