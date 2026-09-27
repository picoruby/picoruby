/* Raspi SDK */
#include <pico/stdlib.h>
#include <pico/bootrom.h>
#include <pico/critical_section.h>
#include <bsp/board.h>
#include <tusb.h>
#include <hardware/clocks.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PicoRuby */
#include "picoruby.h"
#include "picoruby/debug.h"
#if defined(PICORB_VM_MRUBY)
#include <task.h> // mrb_task_value()
#endif
#include "hal.h" // in picoruby-machine
#include "main_task.c"

#if !defined(PICORB_ALLOC_ESTALLOC)
#error "R2P2 RP2040 port requires PICORB_ALLOC_ESTALLOC"
#endif

#if defined(PICORB_VM_MRUBYC) && !defined(MRBC_ALLOC_LIBC)
#error "R2P2 RP2040 FemtoRuby port requires MRBC_ALLOC_LIBC for Estalloc"
#endif

#if defined(PICORB_VM_MRUBY)
#include "../../picoruby-machine/include/estalloc_mruby.h"
#endif

#include "../../picoruby-machine/include/picorb_heap.h"

static critical_section_t heap_critsec;

static void
heap_enter_critical(void)
{
  critical_section_enter_blocking(&heap_critsec);
}

static void
heap_exit_critical(void)
{
  critical_section_exit(&heap_critsec);
}

/*
 * RAM layout
 *
 * The Ruby heap is not a fixed array. It takes all the RAM the linker left
 * between the end of .bss (the linker symbol `end`) and the top of general
 * RAM (`__StackLimit`), minus a reserve at the top for the newlib heap:
 *
 *   .data/.bss | Estalloc heap (auto-sized) | newlib reserve | core 0 stack
 *   ^end                                    ^__StackLimit - reserve
 *
 * Static data that a build links in (the CYW43 driver, lwIP pools, a new
 * gem) shrinks the heap by itself, so no per-board size table is needed.
 * The core 0 stack is placed by the linker script: on RP2040 in scratch
 * RAM, on RP2350 in the STACK region above `__StackLimit`. Core 1 always
 * uses SCRATCH_X.
 *
 * The newlib reserve receives everything libc allocates through _sbrk.
 * With the default shared allocation (heap_wrap.c) that is only what is
 * allocated before the Ruby heap exists; measured on a Pico 2 W nothing
 * is, so the reserve is insurance. With R2P2_NO_SHARED_ALLOC the pico-sdk
 * and its libraries keep using newlib, so the reserve has to hold their
 * whole working set.
 */
extern char end;          /* first byte past .bss */
extern char __StackLimit; /* top of general RAM */

#if !defined(R2P2_NEWLIB_HEAP_RESERVE)
  #if defined(R2P2_NO_SHARED_ALLOC)
    #define R2P2_NEWLIB_HEAP_RESERVE (64 * 1024)
  #else
    #define R2P2_NEWLIB_HEAP_RESERVE (4 * 1024)
  #endif
#endif

/* Refuse to start with less than this. It means the static data grew so
 * much that the board cannot run Ruby anyway; fail loudly instead of
 * dying in the first allocation. */
#if !defined(R2P2_HEAP_MIN_SIZE)
  #define R2P2_HEAP_MIN_SIZE (64 * 1024)
#endif

/*
 * Replace the weak pico-sdk _sbrk, which grows the newlib heap from `end`
 * straight through the Ruby heap. The newlib heap is a bounded strip at
 * the top of RAM instead. A request that does not fit returns -1, so
 * malloc returns NULL rather than corrupting the Ruby heap.
 *
 * newlib_heap_end is not static so a debugger can read how much of the
 * reserve is in use: p newlib_heap_end - (&__StackLimit - reserve)
 */
char *newlib_heap_end;

void *
_sbrk(int incr)
{
  char *limit = &__StackLimit;
  char *base = limit - R2P2_NEWLIB_HEAP_RESERVE;
  if (newlib_heap_end == NULL) {
    newlib_heap_end = base;
  }
  char *prev = newlib_heap_end;
  char *next = newlib_heap_end + incr;
  if (next > limit || next < base) {
    return (void *)-1;
  }
  newlib_heap_end = next;
  return prev;
}

static void *heap_base;
static size_t heap_size;

/* Compute the Ruby heap window. Estalloc needs an 8-byte aligned base. */
static void
heap_locate(void)
{
  uintptr_t lo = ((uintptr_t)&end + 7u) & ~(uintptr_t)7u;
  uintptr_t hi = (uintptr_t)&__StackLimit - R2P2_NEWLIB_HEAP_RESERVE;
  hi &= ~(uintptr_t)7u;
  if (hi <= lo || hi - lo < R2P2_HEAP_MIN_SIZE) {
    printf("R2P2 FATAL: only %u bytes left for the Ruby heap (need %u)\n",
           (unsigned)(hi > lo ? hi - lo : 0), (unsigned)R2P2_HEAP_MIN_SIZE);
    panic("Ruby heap too small");
  }
  heap_base = (void *)lo;
  heap_size = (size_t)(hi - lo);
}

#if defined(PICORB_VM_MRUBY)
  extern mrb_state *global_mrb; /* defined in mruby-compiler (ccontext.c) */
#endif

static void
gpio_set_in_pull_up(uint pin)
{
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_up(pin);
}

static void
gpio_set_in_pull_down(uint pin)
{
  gpio_init(pin);
  gpio_set_dir(pin, GPIO_IN);
  gpio_pull_down(pin);
}

// All GPIOs will be input to maximize safety for
// possible peripheral for PicoRuby official Board (PRB)
static void
gpio_init_safe(void)
{
#if !defined(PICORB_DEBUG)
  // --- UART ---
  // GPIO0: UART_TX
  gpio_set_in_pull_up(0);
  gpio_set_in_pull_up(1);
#endif

  // --- SPI (2-5) ---
  for (int pin = 2; pin <= 4; pin++) {
    gpio_set_in_pull_down(pin);
  }
  gpio_set_in_pull_up(5); // CS: Unselect

  // --- I2C (6-7, 8-9) ---
  // IN & PULLUP: emulate open-drain output
  for (int pin = 6; pin <= 9; pin++) {
    gpio_set_in_pull_up(pin);
  }

  // --- PWM * 2 (10-11) ---
  gpio_set_in_pull_down(10);
  gpio_set_in_pull_down(11);

  // --- DAC (12-15) ---
  for (int pin = 12; pin <= 15; pin++) {
    gpio_set_in_pull_down(pin);
  }

  // --- LED (16) ---
  gpio_set_in_pull_down(16);

  // --- SWITCH, ENCODER, ADC (17-29) ---
  for (int pin = 17; pin <= 29; pin++) {
    gpio_set_in_pull_down(pin);
  }
}

#if defined(PICORB_VM_MRUBY)
/*
 * A startup error happens before the task scheduler runs, so tud_task() never
 * pumps USB CDC and anything printed here may never reach a console. Instead
 * stash the failure in these file-scope variables and abort(): under a debugger
 * the target halts at a defined point with the phase, exception class name and
 * exception object all readable (inspect startup_error_* / dig into
 * startup_error_exc for the message and backtrace); on a device with no
 * debugger it fails fast instead of pretending to boot.
 */
static const char *startup_error_phase;
static const char *startup_error_class;
static mrb_value    startup_error_exc;

static void
report_startup_error(mrb_state *mrb, mrb_value exc, const char *phase)
{
  startup_error_phase = phase;
  startup_error_class = mrb_obj_classname(mrb, exc);
  startup_error_exc   = exc;
  abort();
}
#endif

int
main(void)
{
  stdio_init_all();
  // printf() goes to Picoprobe UART
  printf("R2P2 PicoRuby starting...\n");
  heap_locate();
  printf("Heap size: %u KB (%p-%p), newlib reserve %u KB\n",
         (unsigned)(heap_size / 1024), heap_base,
         (void *)((char *)heap_base + heap_size),
         (unsigned)(R2P2_NEWLIB_HEAP_RESERVE / 1024));
  board_init();

  gpio_init_safe();

  int ret = 0;

#if defined(PICORB_VM_MRUBY)
  mrb_state *mrb = mrb_open_with_custom_alloc(heap_base, heap_size);
  if (mrb == NULL) {
    /* Heap init or mrb_state allocation failed: there is no VM to print with. */
    const char *msg = "[R2P2] FATAL: mrb_open_with_custom_alloc failed\n";
    picorb_hal_write(1, msg, strlen(msg));
    return 1;
  }
  critical_section_init(&heap_critsec);
  picorb_heap_set_critical_section(heap_enter_critical, heap_exit_critical);
  global_mrb = mrb;

  mrc_ccontext *cc = NULL;
  if (mrb->exc) {
    /* Core / mrblib / gem initialization raised. mrb_open() returns the
       partially-initialized mrb with mrb->exc set for the caller to inspect,
       e.g. a NoMemoryError, or a NoMethodError from a broken gem's Ruby init. */
    report_startup_error(mrb, mrb_obj_value(mrb->exc), "VM initialization");
    ret = 1;
  }
  else {
    mrc_irep *irep = mrb_read_irep(mrb, main_task);
    cc = mrc_ccontext_new(mrb);
    mrb_value name = mrb_str_new_lit(mrb, "R2P2");
    mrb_value task = mrc_create_task(cc, irep, name, mrb_nil_value(), mrb_obj_value(mrb->top_self));
    if (mrb_nil_p(task)) {
      const char *msg = "mrbc_create_task failed\n";
      picorb_hal_write(1, msg, strlen(msg));
      ret = 1;
    }
    else {
      mrb_task_run(mrb);
      if (mrb->exc) {
        /* An exception propagated out of the scheduler itself. */
        report_startup_error(mrb, mrb_obj_value(mrb->exc), "task scheduler");
        ret = 1;
      }
      else {
        /* A task-body exception is stored as the task's result (not mrb->exc)
           via the exception-as-result path, so mrb_task_run() returns cleanly
           when the R2P2 main task dies. Surface it here. */
        mrb_value result = mrb_task_value(mrb, task);
        if (mrb_exception_p(result)) {
          report_startup_error(mrb, result, "R2P2 main task");
          ret = 1;
        }
      }
    }
  }
  mrb_close(mrb);
  if (cc) {
    mrc_ccontext_free(cc);
  }
#elif defined(PICORB_VM_MRUBYC)
  PICORB_ESTALLOC_MRUBYC_INIT(heap_base, heap_size);
  critical_section_init(&heap_critsec);
  picorb_heap_set_critical_section(heap_enter_critical, heap_exit_critical);
  mrbc_init(heap_base, heap_size);
  mrbc_tcb *main_tcb = mrbc_create_task(main_task, 0);
  if (!main_tcb) {
    const char *msg = "mrbc_create_task failed\n";
    picorb_hal_write(1, msg, strlen(msg));
    ret = 1;
  }
  else {
    mrbc_set_task_name(main_tcb, "main_task");
    mrbc_vm *vm = &main_tcb->vm;
    picoruby_init_require(vm);
    mrbc_run();
  }
#endif
  return ret;
}
