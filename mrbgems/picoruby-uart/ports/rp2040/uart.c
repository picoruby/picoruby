#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

#include "../../include/uart.h"

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
  if (strcmp(name, "RP2040_UART0") == 0) {
    return PICORUBY_UART_RP2040_UART0;
  } else if (strcmp(name, "RP2040_UART1") == 0) {
    return PICORUBY_UART_RP2040_UART1;
  } else {
    return ERROR_INVALID_UNIT;
  }
}

void
UART_init(int unit_num, uint32_t txd_pin, uint32_t rxd_pin, RingBuffer *ring_buffer)
{
  uint irq = UART0_IRQ;
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_init(unit, DEFAULT_BAUDRATE);

  gpio_set_function(txd_pin, GPIO_FUNC_UART);
  gpio_set_function(rxd_pin, GPIO_FUNC_UART);

  if (unit_num == PICORUBY_UART_RP2040_UART0) {
    irq = UART0_IRQ;
    irq_set_exclusive_handler(irq, on_uart0_rx);
    rx_buffers[0] = ring_buffer;
  } else if (unit_num == PICORUBY_UART_RP2040_UART1) {
    irq = UART1_IRQ;
    irq_set_exclusive_handler(irq, on_uart1_rx);
    rx_buffers[1] = ring_buffer;
  }
  irq_set_enabled(irq, true);
  uart_set_irq_enables(unit, true, false);
}

uint32_t
UART_set_baudrate(int unit_num, uint32_t baudrate)
{
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  return uart_set_baudrate(unit, baudrate);
}

void
UART_set_flow_control(int unit_num, bool cts, bool rts)
{
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_set_hw_flow(unit, cts, rts);
}

void
UART_set_format(int unit_num, uint32_t data_bits, uint32_t stop_bits, uint8_t parity)
{
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
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  return uart_is_writable(unit);
}

void
UART_write_blocking(int unit_num, const uint8_t *src, size_t len)
{
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_write_blocking(unit, src, len);
}

bool
UART_is_readable(int unit_num)
{
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  return uart_is_readable(unit);
}

size_t
UART_read_nonblocking(int unit_num, uint8_t *dst, size_t maxlen)
{
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
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_set_break(unit, true);
  sleep_ms(interval);
  uart_set_break(unit, false);
}

void
UART_flush(int unit_num)
{
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  uart_tx_wait_blocking(unit);
}

void
UART_clear_rx_buffer(int unit_num)
{
  uart_inst_t *unit = NULL;
  UNIT_SELECT();
  while (uart_is_readable(unit)) {
    uart_getc(unit);
  }
}

void
UART_clear_tx_buffer(int unit_num)
{
  // no-op
}
