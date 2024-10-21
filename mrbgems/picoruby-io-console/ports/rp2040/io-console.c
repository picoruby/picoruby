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

#define ALARM_IRQ 0

#ifndef MRBC_TICK_UNIT
#define MRBC_TICK_UNIT 1
#endif

#ifndef MRBC_NO_TIMER
struct repeating_timer timer;

bool
alarm_irq(struct repeating_timer *t)
{
  mrbc_tick();
  return true;
}

void
hal_init(void)
{
  add_repeating_timer_ms(MRBC_TICK_UNIT, alarm_irq, NULL, &timer);
  clocks_hw->sleep_en0 = 0;
  clocks_hw->sleep_en1 = CLOCKS_SLEEP_EN1_CLK_SYS_TIMER_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_USB_USBCTRL_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART0_BITS
  | CLOCKS_SLEEP_EN1_CLK_SYS_UART1_BITS
  | CLOCKS_SLEEP_EN1_CLK_PERI_UART1_BITS;
}

void
hal_enable_irq()
{
  irq_set_enabled(ALARM_IRQ, true);
}

void
hal_disable_irq()
{
  irq_set_enabled(ALARM_IRQ, false);
}

void
hal_idle_cpu()
{
  __wfi();
}

#else // MRBC_NO_TIMER

void
hal_idle_cpu()
{
  sleep_ms(MRBC_TICK_UNIT);
  mrbc_tick();
}
#endif

int hal_write(int fd, const void *buf, int nbytes)
{
  tud_task();
  tud_cdc_write(buf, nbytes);
  return tud_cdc_write_flush();
}

int hal_flush(int fd) {
  return tud_cdc_write_flush();
}

int
hal_read_available(void)
{
  if (tud_cdc_available()) {
    return 1;
  } else {
    return 0;
  }
}

int
hal_getchar(void)
{
  tud_task();
  if (tud_cdc_available()) {
    return tud_cdc_read_char();
  } else {
    return -1;
  }
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
 *  Global buffer
 *
 *------------------------------------*/

#define BUF_SIZE 10
static char *stdin_buf = NULL;
static size_t stdin_buf_capa = BUF_SIZE;
static size_t stdin_buf_pos = 0;

static bool raw_mode = false;
static bool echo_back = true;

static void
init_stdin_buf(mrbc_vm *vm)
{
  if (stdin_buf) return;
  stdin_buf_pos = 0;
  stdin_buf_capa = BUF_SIZE;
  stdin_buf = (char *)mrbc_alloc(vm, stdin_buf_capa);
}

static void
clear_stdin_buf(mrbc_vm *vm)
{
  stdin_buf_pos = 0;
  stdin_buf_capa = BUF_SIZE;
  mrbc_free(vm, stdin_buf);
  stdin_buf = NULL;
}

static int
getc_from_stdin_buf(mrbc_vm *vm, mrbc_value *v)
{
  init_stdin_buf(vm);
  if (stdin_buf_capa - 1 <= stdin_buf_pos) {
    stdin_buf_capa += BUF_SIZE;
    stdin_buf = (char *)mrbc_realloc(vm, stdin_buf, stdin_buf_capa);
    if (!stdin_buf) {
      mrbc_raise(vm, MRBC_CLASS(IOError), "memory allocation error");
      return -3;
    }
  }
  int c = hal_getchar();
  // Not available
  if (c == -1) return -1;

  if (!raw_mode) {
    // Ctrl+C
    if (c == 3) {
      clear_stdin_buf(vm);
      mrbc_raise(vm, MRBC_CLASS(IOError), "Interrupted");
      return -2;
    }
    // Ctrl+D
    if (c == 4) {
      SET_NIL_RETURN();
      return -1;
    }
    // Backspace
    if (c == 8 || c == 127) {
      if (0 < stdin_buf_pos) {
        stdin_buf_pos--;
        if (echo_back) hal_write(1, "\b \b", 3);
      }
      return 0;
    }
  }
  if (echo_back) hal_write(1, &c, 1); // echo back
  stdin_buf[stdin_buf_pos++] = c;
  return c;
}

/*-------------------------------------
 *
 *  class IO
 *
 *------------------------------------*/

static void
c_getc(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int c;
  mrbc_value str;
  if (stdin_buf_pos == 0) {
    while (1) {
      c = getc_from_stdin_buf(vm, v);
      if (c < -1) return;
      if (c == -1) continue;
      if (c == 0) continue;
      if (raw_mode || c == '\n') break;
    }
  }
  str = mrbc_string_new(vm, stdin_buf, 1);
  SET_RETURN(str);
  memmove(stdin_buf, stdin_buf + 1, stdin_buf_pos--);
}

static void
c_gets(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int c;
  while (1) {
    for (int i = 0; i < stdin_buf_pos; i++) {
      if (stdin_buf[i] == '\n') {
        mrbc_value str = mrbc_string_new(vm, stdin_buf, i+1);
        clear_stdin_buf(vm);
        SET_RETURN(str);
        return;
      }
    }
    c = getc_from_stdin_buf(vm, v);
    if (c == -1) continue;
    if (c == 0) continue;
    if (c < 0) return;
  }
}

void
c_raw_bang(mrb_vm *vm, mrb_value *v, int argc)
{
  raw_mode = true;
}

void
c_cooked_bang(mrb_vm *vm, mrb_value *v, int argc)
{
  raw_mode = false;
}

static void
c_echo_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[1].tt == MRBC_TT_TRUE) {
    echo_back = true;
  } else {
    echo_back = false;
  }
  SET_RETURN(v[1]);
}

static void
c_echo_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (echo_back) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
io_console_port_init(mrbc_vm *vm, mrbc_class *class_IO)
{
  mrbc_define_method(vm, class_IO, "raw!", c_raw_bang);
  mrbc_define_method(vm, class_IO, "cooked!", c_cooked_bang);
  mrbc_define_method(vm, class_IO, "getc", c_getc);
  mrbc_define_method(vm, class_IO, "echo=", c_echo_eq);
  mrbc_define_method(vm, class_IO, "echo?", c_echo_q);
  mrbc_define_method(vm, mrbc_class_object, "gets", c_gets);
}

