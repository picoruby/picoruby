#include <string.h>
#include <stdlib.h>

/* pico-sdk and tinyusb */
#include <pico/stdlib.h>
#include <hardware/irq.h>
#include <hardware/clocks.h>
#include <hardware/sync.h>
#include <tusb.h>

/* mruby/c */
#include <mrubyc.h>

/*-------------------------------------
 *
 * HAL
 *
 *------------------------------------*/

#ifdef MRBC_NO_TIMER
  #error "MRBC_NO_TIMER is not supported"
#endif

#define ALARM_NUM 0
#define ALARM_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM)

#ifndef MRBC_TICK_UNIT
#define MRBC_TICK_UNIT 1
#endif
#define US_PER_MS (MRBC_TICK_UNIT * 1000)

struct repeating_timer timer;

static volatile uint32_t interrupt_nesting = 0;

void hal_enable_irq(void);
void hal_disable_irq(void);

static volatile bool in_tick_processing = false;


#define DEBUG_TICK_PIN 14    // CH1: tick発生タイミング
#define DEBUG_PROC_PIN 15    // CH2: 処理状態表示用（用途を切り替えて観察）

static void
alarm_handler(void)
{
gpio_put(DEBUG_TICK_PIN, 1);    // 割り込み開始

  if (in_tick_processing) {
    timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + US_PER_MS;
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    return;
  }

  in_tick_processing = true;
  __dmb();

  uint32_t current_time = timer_hw->timerawl;
  uint32_t next_time = current_time + US_PER_MS;
  timer_hw->alarm[ALARM_NUM] = next_time;
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

  mrbc_tick();

  __dmb();
  in_tick_processing = false;
gpio_put(DEBUG_TICK_PIN, 0);    // 割り込み終了
}

static void
usb_irq_handler(void) {
//gpio_put(DEBUG_PROC_PIN, 1);    // USB処理開始
  if (!tud_inited()) {
    return;
  }
  tud_task();
//gpio_put(DEBUG_PROC_PIN, 0);    // USB処理終了
}

void
hal_init(void)
{
    gpio_init(DEBUG_TICK_PIN);
    gpio_init(DEBUG_PROC_PIN);
    gpio_set_dir(DEBUG_TICK_PIN, GPIO_OUT);
    gpio_set_dir(DEBUG_PROC_PIN, GPIO_OUT);
    gpio_put(DEBUG_TICK_PIN, 0);
    gpio_put(DEBUG_PROC_PIN, 0);

  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  irq_set_exclusive_handler(ALARM_IRQ, alarm_handler);
  irq_set_enabled(ALARM_IRQ, true);
  irq_set_priority(ALARM_IRQ, PICO_HIGHEST_IRQ_PRIORITY);
  timer_hw->alarm[ALARM_NUM] = timer_hw->timerawl + US_PER_MS;

  clocks_hw->sleep_en0 = 0;
#if defined(PICO_RP2040)
  clocks_hw->sleep_en1 =
  CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_USB_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS;
#elif defined(PICO_RP2350)
  clocks_hw->sleep_en1 =
  CLOCKS_SLEEP_EN1_CLK_SYS_TIMER0_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_TIMER1_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_USB_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS;
#else
  #error "Unsupported Pico Board"
#endif

  tud_init(TUD_OPT_RHPORT);
  irq_add_shared_handler(USBCTRL_IRQ, usb_irq_handler,
      PICO_SHARED_IRQ_HANDLER_LOWEST_ORDER_PRIORITY);
}

void
hal_enable_irq()
{
  if (interrupt_nesting == 0) {
//    return; // wrong state???
  }
  interrupt_nesting--;
  if (interrupt_nesting > 0) {
    return;
  }
  __dmb();
gpio_put(DEBUG_PROC_PIN, 0);    // 割り込み禁止区間終了
  asm volatile ("cpsie i" : : : "memory");
}

void
hal_disable_irq()
{
gpio_put(DEBUG_PROC_PIN, 1);    // 割り込み禁止区間開始
  asm volatile ("cpsid i" : : : "memory");
  __dmb();
  interrupt_nesting++;
}

void
hal_idle_cpu()
{
  hal_disable_irq();
  __dsb();
  __wfi();
  hal_enable_irq();
}

int hal_write(int fd, const void *buf, int nbytes)
{
  hal_disable_irq();
  tud_task();
  tud_cdc_write(buf, nbytes);
  int len = tud_cdc_write_flush();
  hal_enable_irq();
  return len;
}

int hal_flush(int fd) {
  hal_disable_irq();
  tud_task();
  int len = tud_cdc_write_flush();
  hal_enable_irq();
  return len;
}

int
hal_read_available(void)
{
  hal_disable_irq();
  int len = tud_cdc_available();
  hal_enable_irq();
  if (0 < len) {
    return 1;
  } else {
    return 0;
  }
}

int
hal_getchar(void)
{
  int c = -1;
  hal_disable_irq();
  tud_task();
  int len = tud_cdc_available();
  if (0 < len) {
    c = tud_cdc_read_char();
  }
  hal_enable_irq();
  return c;
}

//================================================================
/*!@brief
  abort program

  @param s	additional message.
*/
void hal_abort(const char *s)
{
  if( s ) {
    hal_write(1, s, strlen(s));
  }

  abort();
}


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

