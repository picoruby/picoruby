#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "../src/mrubyc/src/mrubyc.h"

#include "../src/mmrbc.h"
#include "../src/common.h"
#include "../src/compiler.h"
#include "../src/debug.h"
#include "../src/scope.h"
#include "../src/stream.h"

#include "heap.h"
#include "mmirb_lib/shell.c"

int
init_hal_fd(const char *pathname)
{
  int fd = hal_open(pathname, O_RDWR);
  //fd = open(pathname, O_RDWR|O_NONBLOCK);
  if (fd < 0) {
    FATAL("hal_open failed");
    return 1;
  }
  hal_set_fd(fd);
  return 0;
}

void
init_shell_interrupt(void (*handler)(int))
{
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGUSR1);
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = handler;
  act.sa_mask    = sigset;
  act.sa_flags  |= SA_RESTART;
  sigaction(SIGUSR1, &act, 0);
}

static mrbc_tcb *tcb_shell;

/*
 * int no; dummy to avoid warning. it's originally a signal number.
 */
static
void
resume_shell(int no) {
  mrbc_resume_task(tcb_shell);
}

#define BUFLEN 100

static void
c_gets(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char *buf = mmrbc_alloc(BUFLEN + 1);
  int len;
  len = hal_read(1, buf, BUFLEN);
  if (len == -1) {
    FATAL("read");
    SET_NIL_RETURN();
  } else if (len) {
    buf[len] = '\0'; // terminate;
    mrbc_value val = mrbc_string_new(vm, buf, len);
    SET_RETURN(val);
  } else {
    FATAL("This should not happen (c_gets)");
  }
  mmrbc_free(buf);
}

static void
c_pid(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(getpid());
}

static void
c_exit_shell(mrbc_vm *vm, mrbc_value *v, int argc)
{
  /*
   * PENDING
   *
   * you can not call q_delete_task() as it is static function in rrt0.c
   *
  q_delete_task(tcb_shell);
  */
}

void run(uint8_t *mrb)
{
  init_static();
  struct VM *vm = mrbc_vm_open(NULL);
  if( vm == 0 ) {
    FATAL("Error: Can't open VM.");
    return;
  }
  if( mrbc_load_mrb(vm, mrb) != 0 ) {
    FATAL("Error: Illegal bytecode.");
    return;
  }
  mrbc_vm_begin(vm);
  mrbc_vm_run(vm);
  find_class_by_object(vm, vm->current_regs);
  mrbc_value ret = mrbc_send(vm, vm->current_regs, 0, vm->current_regs, "inspect", 0);
  hal_write(1, "=> ", 3);
  hal_write(1, ret.string->data, ret.string->size);
  hal_write(1, "\n", 1);
  mrbc_vm_end(vm);
  mrbc_vm_close(vm);
}

static void
c_compile_and_run(mrbc_vm *vm, mrbc_value *v, int argc)
{
  static Scope *scope;
  scope = Scope_new(NULL);
  StreamInterface *si = StreamInterface_new((char *)GET_STRING_ARG(1), STREAM_TYPE_MEMORY);
  if (Compile(scope, si)) {
    run(scope->vm_code);
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
  StreamInterface_free(si);
}

void
process_parent(pid_t pid)
{
  WARN("successfully forked. parent pid: %d", pid);
  fd_set readfds;
  FD_ZERO(&readfds);
  hal_FD_SET(1, &readfds);
  struct timespec ts;
  ts.tv_sec = 1;
  ts.tv_nsec = 0;
  int ret;
  for (;;) {
    ret = hal_select(1, &readfds, NULL, NULL, NULL);
    if (ret == -1) {
      FATAL("select");
    } else if (ret == 0) {
      FATAL("This should not happen (1)");
    } else if (ret > 0) {
      INFO("Input recieved. Issuing SIGUSR1 to pid %d", pid);
      kill(pid, SIGUSR1);
      ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
      if (ret) FATAL("clock_nanosleep");
    }
  }
}

static uint8_t heap[HEAP_SIZE];

void
process_child(void)
{
  WARN("successfully forked. child");
  mrbc_init(heap, HEAP_SIZE);
  mrbc_define_method(0, mrbc_class_object, "compile_and_run", c_compile_and_run);
  mrbc_define_method(0, mrbc_class_object, "gets", c_gets);
  mrbc_define_method(0, mrbc_class_object, "pid", c_pid);
  mrbc_define_method(0, mrbc_class_object, "exit_shell", c_exit_shell);
  //mrbc_define_method(0, mrbc_class_object, "xmodem", c_xmodem);
  tcb_shell = mrbc_create_task(shell, 0);
  mrbc_run();
  if (close(hal_fd) == -1) {
    FATAL("close");
    return;
  }
}


int
main(int argc, char *argv[])
{
  loglevel = LOGLEVEL_WARN;
  if (init_hal_fd(argv[1]) != 0) {
    return 1;
  }
  init_shell_interrupt(resume_shell);
  pid_t pid = fork();
  if (pid < 0) {
    FATAL("fork failed");
    return 1;
  } else if (pid == 0) {
    process_child();
  } else if (pid > 0) {
    process_parent(pid);
  }
  return 0;
}
