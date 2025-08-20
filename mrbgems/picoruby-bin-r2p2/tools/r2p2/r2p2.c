#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <termios.h>

#include "picoruby.h"
#include "mruby_compiler.h"

#if !defined(PICORB_PLATFORM_POSIX)
#define PICORB_PLATFORM_POSIX 1
#endif
#include "../../../picoruby-machine/include/machine.h"

#include "app.c"

#ifndef HEAP_SIZE
#define HEAP_SIZE (1024 * 2000)
#endif

static uint8_t heap_pool[HEAP_SIZE] __attribute__((aligned(16)));;

// from machine.h
volatile sig_atomic_t sigint_status = MACHINE_SIG_NONE;
int exit_status = 0;

static struct termios orig_termios;

static void
signal_handler(int signum)
{
  switch (signum) {
    case SIGINT:
      if (sigint_status == MACHINE_SIGINT_EXIT) {
        tcsetattr(0, TCSANOW, &orig_termios);
        exit(exit_status);
      }
      sigint_status = MACHINE_SIGINT_RECEIVED;
      break;
    case SIGTSTP:
      sigint_status = MACHINE_SIGTSTP_RECEIVED;
      break;
    default:
      // Ignore other signals
  }
}

static void
init_posix(void)
{
  // Get the original terminal attributes
  tcgetattr(0, &orig_termios);

  // Copy the original attributes to modify them
  struct termios newt = orig_termios;

  // Set the terminal to raw mode
  newt.c_lflag &= ~(ICANON | ECHO | ISIG);
  tcsetattr(0, TCSANOW, &newt);

  // Set up signal handlers
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  // Set the flags to 0 to use default behavior
  sa.sa_flags = 0;

  // Register signal handlers for SIGINT and SIGTSTP
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTSTP, &sa, NULL);
}


#if defined(PICORB_VM_MRUBYC)

int
main(void)
{
  init_posix();

  mrbc_init(heap_pool, HEAP_SIZE);
  mrbc_tcb *tcb = mrbc_create_task(app, 0);
  mrbc_vm *vm = &tcb->vm;
  picoruby_init_require(vm);
  mrbc_run();
  return 0;
}

#elif defined(PICORB_VM_MRUBY)

mrb_state *global_mrb = NULL;

int
main(void)
{
  init_posix();

  int ret = 0;
#if 1
  mrb_state *mrb = mrb_open_with_custom_alloc(heap_pool, HEAP_SIZE);
#else
  mrb_state *mrb = mrb_open();
#endif
  if (mrb == NULL) {
    fprintf(stderr, "mrb_open failed\n");
    return 1;
  }
  global_mrb = mrb;
  mrc_irep *irep = mrb_read_irep(mrb, app);
  mrc_ccontext *cc = mrc_ccontext_new(mrb);
  mrb_value name = mrb_str_new_cstr(mrb, "R2P2");
  mrb_value task = mrc_create_task(cc, irep, name, mrb_nil_value(), mrb_obj_value(mrb->top_self));
  if (mrb_nil_p(task)) {
    fprintf(stderr, "mrbc_create_task failed\n");
    ret = 1;
  }
  else {
    mrb_tasks_run(mrb);
  }
  mrb_close(mrb);
  mrc_ccontext_free(cc);
  return ret;
}

#endif
