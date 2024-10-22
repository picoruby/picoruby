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

static bool raw_mode_saved = false;
static bool raw_mode_current = false;
static bool echo_back_saved = true;
static bool echo_back_current = true;

static int
getc_from_stdin_buf(mrbc_vm *vm, mrbc_value *v)
{
  static char buf[32];
  static int buf_size = 0;

  while (1) {
    int c = hal_getchar();
    // Not available
    if (c == -1) break;

    if (!raw_mode_current) {
      // Ctrl+C
      if (c == 3) {
        mrbc_raise(vm, MRBC_CLASS(IOError), "Interrupted");
        return -2;
      }
      // Ctrl+D
      if (c == 4) {
        SET_NIL_RETURN();
        return -1;
      }
      // ESC
      if (c == 27) {
        continue;
      }
      // Backspace
      if (c == 8 || c == 127) {
        if (echo_back_current) hal_write(1, "\b \b", 3);
        if (buf_size > 0) {
          buf_size--;
        }
        return c;
      }
    }
    if (buf_size == sizeof(buf)) {
      mrbc_raise(vm, MRBC_CLASS(IOError), "Buffer overflow");
      return -3;
    }
    if (echo_back_current) hal_write(1, &c, 1); // echo back
    buf[buf_size++] = c;
  }
  if (buf_size == 0) return -1;
  int c = (int)buf[0];
  memmove(buf, buf + 1, buf_size - 1);
  buf_size--;
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
  int f = -1;
  mrbc_value str;
  while (1) {
    c = getc_from_stdin_buf(vm, v);
    if (f == -1) f = c;
    if (c < -1) return;
    if (c == 0) continue;
    if (c == -1) continue;
    if (c == 8 || c == 127) { // Backspace
      if (raw_mode_current) break;
      continue;
    }
    if (raw_mode_current || c == '\n') break;
  }
  assert(f != -1);
  str = mrbc_string_new(vm, (char *)&f, 1);
  SET_RETURN(str);
}

static void
c_gets(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int c;
  mrbc_value str = mrbc_string_new(vm, NULL, 0);
  while (1) {
    c = getc_from_stdin_buf(vm, v);
    if (c == -1) continue;
    if (c == 0) continue;
    if (c < 0) return;
    if (c == 8 || c == 127) {
      if (0 < str.string->size) {
        str.string->size--;
        mrbc_realloc(vm, str.string->data, str.string->size);
      }
    } else {
      mrbc_string_append_cstr(&str, (char *)&c);
    }
    if (c == '\n') break;
  }
  SET_RETURN(str);
}

void
c_raw_bang(mrb_vm *vm, mrb_value *v, int argc)
{
  echo_back_saved = echo_back_current;
  raw_mode_saved = raw_mode_current;
  raw_mode_current = true;
}

void
c_cooked_bang(mrb_vm *vm, mrb_value *v, int argc)
{
  echo_back_saved = echo_back_current;
  raw_mode_saved = raw_mode_current;
  raw_mode_current = false;
}

void
c__restore_termios(mrb_vm *vm, mrb_value *v, int argc)
{
  echo_back_current = echo_back_saved;
  raw_mode_current = raw_mode_saved;
}

static void
c_echo_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[1].tt == MRBC_TT_TRUE) {
    echo_back_current = true;
  } else {
    echo_back_current = false;
  }
  SET_RETURN(v[1]);
}

static void
c_echo_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (echo_back_current) {
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
  mrbc_define_method(vm, class_IO, "_restore_termios", c__restore_termios);
  mrbc_define_method(vm, class_IO, "getc", c_getc);
  mrbc_define_method(vm, class_IO, "echo=", c_echo_eq);
  mrbc_define_method(vm, class_IO, "echo?", c_echo_q);
  mrbc_define_method(vm, mrbc_class_object, "gets", c_gets);
}

