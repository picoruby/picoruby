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
#include <assert.h>

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

#if !defined(HEAP_SIZE)
  #if defined(PICO_RP2040)
    #define RAM_SIZE_KB             264
  #elif defined(PICO_RP2350)
    /*
     * RP2350 has 512KB of regular SRAM plus 8KB of scratch SRAM. R2P2's
     * RP2350 linker script reserves scratch SRAM for core0 stack, so heap_pool
     * must be sized against regular SRAM only.
     */
    #define RAM_SIZE_KB             512
  #else
    #error "PICO_RP2040 or PICO_RP2350 must be defined"
  #endif
  #if defined(USE_WIFI) && defined(R2P2_NO_SHARED_ALLOC)
    /*
     * When libc/newlib allocation is separated from Estalloc, keep room for
     * CYW43/LwIP/mbedTLS outside heap_pool. In shared mode, libc allocation is
     * routed to Estalloc too, so reserving this RAM would only shrink the
     * shared heap.
     */
    #if defined(PICO_RP2040)
      #define WIFI_RESERVED_SIZE_KB  32
    #else
      #define WIFI_RESERVED_SIZE_KB  64
    #endif
  #else
    #define WIFI_RESERVED_SIZE_KB    20
  #endif
  // Compiling a big Ruby code may need more stack size
  #define BASIC_STACK_SIZE_KB   80
  #if defined(USE_WIFI)
    #define STACK_SIZE_KB (BASIC_STACK_SIZE_KB + WIFI_RESERVED_SIZE_KB)
  #else
    #define STACK_SIZE_KB BASIC_STACK_SIZE_KB
  #endif
  #define HEAP_SIZE_KB (RAM_SIZE_KB - STACK_SIZE_KB)
  #define HEAP_SIZE (HEAP_SIZE_KB * 1024)
#endif

static uint8_t heap_pool[HEAP_SIZE] __attribute__((aligned(8)));

#if defined(PICORB_VM_MRUBY)
  extern mrb_state *global_mrb; /* defined in mruby-compiler (ccontext.c) */
#endif

/* Linker symbol: bottom of C stack (top of heap region) */
extern uint8_t __StackBottom[];

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
  printf("Heap size: %d KB\n", HEAP_SIZE_KB);
  board_init();

  gpio_init_safe();

  assert((uint8_t *)heap_pool + HEAP_SIZE <= (uint8_t *)__StackBottom
         && "heap_pool overlaps with C stack");

  int ret = 0;

#if defined(PICORB_VM_MRUBY)
  mrb_state *mrb = mrb_open_with_custom_alloc(heap_pool, HEAP_SIZE);
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
  PICORB_ESTALLOC_MRUBYC_INIT(heap_pool, HEAP_SIZE);
  critical_section_init(&heap_critsec);
  picorb_heap_set_critical_section(heap_enter_critical, heap_exit_critical);
  mrbc_init(heap_pool, HEAP_SIZE);
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
