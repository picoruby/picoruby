#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "../../include/uart.h"

/* Bitbang UART state */
typedef struct {
  uint32_t baudrate;
  int8_t   pin;
  int8_t   de_pin;
  uint8_t  data_bits;
  uint8_t  stop_bits;
  uint8_t  parity;
} uart_bitbang_info_t;

static uart_bitbang_info_t bitbang_info = {
  .baudrate  = 9600,
  .pin       = -1,
  .de_pin    = -1,
  .data_bits = 8,
  .stop_bits = 1,
  .parity    = PARITY_NONE,
};

#define UNIT_SELECT() \
  do { \
    switch (unit_num) { \
      case PICORUBY_UART_RP2040_UART0: { unit = uart0; break; } \
      case PICORUBY_UART_RP2040_UART1: { unit = uart1; break; } \
      default: { /* TODO: raise error */ } \
    } \
  } while (0)

static RingBuffer *rx_buffers[2];

static void
on_uart0_rx(void)
{
  while (uart_is_readable(uart0)) {
    uint8_t ch = uart_getc(uart0);
    UART_pushBuffer(rx_buffers[0], ch);
  }
}

static void
on_uart1_rx(void)
{
  while (uart_is_readable(uart1)) {
    uint8_t ch = uart_getc(uart1);
    UART_pushBuffer(rx_buffers[1], ch);
  }
}

int
UART_unit_name_to_unit_num(const char *name)
{
  if (strcmp(name, "BITBANG") == 0) {
    return PICORUBY_UART_BITBANG;
  } else if (strcmp(name, "RP2040_UART0") == 0) {
    return PICORUBY_UART_RP2040_UART0;
  } else if (strcmp(name, "RP2040_UART1") == 0) {
    return PICORUBY_UART_RP2040_UART1;
  } else {
    return UART_ERROR_INVALID_UNIT;
  }
}

void
UART_init(int unit_num, uint32_t txd_pin, uint32_t rxd_pin, RingBuffer *ring_buffer)
{
  if (unit_num == PICORUBY_UART_BITBANG) {
    bitbang_info.pin    = (int8_t)txd_pin;
    bitbang_info.de_pin = (int8_t)(int32_t)rxd_pin; /* rxd_pin carries de_pin (-1 if unused) */
    gpio_init(bitbang_info.pin);
    gpio_pull_up(bitbang_info.pin);
    gpio_set_dir(bitbang_info.pin, GPIO_IN);
    if (bitbang_info.de_pin >= 0) {
      gpio_init(bitbang_info.de_pin);
      gpio_set_dir(bitbang_info.de_pin, GPIO_OUT);
      gpio_put(bitbang_info.de_pin, 0); /* DE low = receive mode */
    }
    return;
  }

  uint irq = UART0_IRQ;
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_init(unit, DEFAULT_BAUDRATE);

  gpio_set_function(txd_pin, GPIO_FUNC_UART);
  gpio_set_function(rxd_pin, GPIO_FUNC_UART);

  int buf_index = unit_num - 1; /* RP2040_UART0=1 → index 0, RP2040_UART1=2 → index 1 */
  if (unit_num == PICORUBY_UART_RP2040_UART0) {
    irq = UART0_IRQ;
    irq_set_exclusive_handler(irq, on_uart0_rx);
    rx_buffers[buf_index] = ring_buffer;
  } else if (unit_num == PICORUBY_UART_RP2040_UART1) {
    irq = UART1_IRQ;
    irq_set_exclusive_handler(irq, on_uart1_rx);
    rx_buffers[buf_index] = ring_buffer;
  }
  irq_set_enabled(irq, true);
  uart_set_irq_enables(unit, true, false);
}

uint32_t
UART_set_baudrate(int unit_num, uint32_t baudrate)
{
  if (unit_num == PICORUBY_UART_BITBANG) {
    bitbang_info.baudrate = baudrate;
    return baudrate;
  }
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  return uart_set_baudrate(unit, baudrate);
}

void
UART_set_flow_control(int unit_num, bool cts, bool rts)
{
  if (unit_num == PICORUBY_UART_BITBANG) {
    return; /* no hardware flow control for bitbang */
  }
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_set_hw_flow(unit, cts, rts);
}

void
UART_set_format(int unit_num, uint32_t data_bits, uint32_t stop_bits, uint8_t parity)
{
  if (unit_num == PICORUBY_UART_BITBANG) {
    bitbang_info.data_bits = (uint8_t)data_bits;
    bitbang_info.stop_bits = (uint8_t)stop_bits;
    bitbang_info.parity    = parity;
    return;
  }
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_set_format(unit, data_bits, stop_bits, (uart_parity_t)parity);
}

void
UART_set_function(uint32_t pin)
{
  gpio_set_function(pin, GPIO_FUNC_UART);
}

bool
UART_is_writable(int unit_num)
{
  if (unit_num == PICORUBY_UART_BITBANG) return true;
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  return uart_is_writable(unit);
}

/* --- Bitbang helper: transmit one byte (LSB first) --- */
static void
bitbang_write_byte(uint8_t byte)
{
  uint32_t bit_us = 1000000 / bitbang_info.baudrate;
  uint32_t t = time_us_32();

  gpio_put(bitbang_info.pin, 0); /* start bit */
  t += bit_us;
  while (time_us_32() < t);

  for (int i = 0; i < bitbang_info.data_bits; i++) {
    gpio_put(bitbang_info.pin, (byte >> i) & 1);
    t += bit_us;
    while (time_us_32() < t);
  }

  gpio_put(bitbang_info.pin, 1); /* stop bit(s) */
  t += bit_us * bitbang_info.stop_bits;
  while (time_us_32() < t);
}

void
UART_write_blocking(int unit_num, const uint8_t *src, size_t len)
{
  if (unit_num == PICORUBY_UART_BITBANG) {
    for (size_t i = 0; i < len; i++) {
      bitbang_write_byte(src[i]);
    }
    return;
  }
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_write_blocking(unit, src, len);
}

bool
UART_is_readable(int unit_num)
{
  if (unit_num == PICORUBY_UART_BITBANG) return false;
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  return uart_is_readable(unit);
}

size_t
UART_read_nonblocking(int unit_num, uint8_t *dst, size_t maxlen)
{
  if (unit_num == PICORUBY_UART_BITBANG) return 0;
  size_t actual_len = 0;
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  while (uart_is_readable(unit)) {
    dst[actual_len++] = uart_getc(unit);
    if (maxlen <= actual_len) {
      break;
    }
  }
  return actual_len;
}

void
UART_break(int unit_num, uint32_t interval)
{
  if (unit_num == PICORUBY_UART_BITBANG) {
    /* drive line LOW for interval ms */
    gpio_set_dir(bitbang_info.pin, GPIO_OUT);
    gpio_put(bitbang_info.pin, 0);
    sleep_ms(interval);
    gpio_put(bitbang_info.pin, 1);
    gpio_set_dir(bitbang_info.pin, GPIO_IN);
    return;
  }
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_set_break(unit, true);
  sleep_ms(interval);
  uart_set_break(unit, false);
}

void
UART_flush(int unit_num)
{
  if (unit_num == PICORUBY_UART_BITBANG) return; /* synchronous, nothing to flush */
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_tx_wait_blocking(unit);
}

void
UART_clear_rx_buffer(int unit_num)
{
  if (unit_num == PICORUBY_UART_BITBANG) return;
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  while (uart_is_readable(unit)) {
    uart_getc(unit);
  }
}

void
UART_clear_tx_buffer(int unit_num)
{
  /* no-op */
}

/* --- Bitbang direction control --- */

void
UART_bitbang_tx_mode(void)
{
  if (bitbang_info.de_pin >= 0) {
    gpio_put(bitbang_info.de_pin, 1); /* assert DE before switching pin */
  }
  gpio_set_dir(bitbang_info.pin, GPIO_OUT);
  gpio_put(bitbang_info.pin, 1); /* idle high */
}

void
UART_bitbang_rx_mode(void)
{
  gpio_set_dir(bitbang_info.pin, GPIO_IN);
  if (bitbang_info.de_pin >= 0) {
    gpio_put(bitbang_info.de_pin, 0); /* deassert DE after releasing pin */
  }
}

/* --- Bitbang RX: receive one byte with timeout --- */
static int
bitbang_read_byte(uint8_t *out, uint32_t deadline_us)
{
  uint32_t bit_us = 1000000 / bitbang_info.baudrate;

  /* wait for falling edge (start bit) */
  if (deadline_us > 0) {
    while (gpio_get(bitbang_info.pin) != 0) {
      if (time_us_32() >= deadline_us) return -1; /* timeout */
    }
  } else {
    while (gpio_get(bitbang_info.pin) != 0);
  }

  uint32_t t_detect = time_us_32();
  uint8_t byte = 0;

  /* sample each data bit at its center: t_detect + bit_us*(i+1) + bit_us/2 */
  for (int i = 0; i < bitbang_info.data_bits; i++) {
    uint32_t sample_time = t_detect + bit_us * (uint32_t)(i + 1) + bit_us / 2;
    while (time_us_32() < sample_time);
    byte |= (uint8_t)(gpio_get(bitbang_info.pin) << i); /* LSB first */
  }

  /* wait through stop bit(s) */
  uint32_t stop_time = t_detect + bit_us * (uint32_t)(bitbang_info.data_bits + 1 + bitbang_info.stop_bits);
  while (time_us_32() < stop_time);

  *out = byte;
  return 0;
}

int
UART_bitbang_read_blocking(uint8_t *dst, size_t len, uint32_t timeout_ms)
{
  uint32_t deadline = (timeout_ms > 0) ? (time_us_32() + timeout_ms * 1000) : 0;
  for (size_t i = 0; i < len; i++) {
    if (bitbang_read_byte(&dst[i], deadline) < 0) {
      return (int)i; /* return bytes successfully read before timeout */
    }
  }
  return (int)len;
}
