#include <stddef.h>
#include "../../include/uart.h"

int
UART_unit_name_to_unit_num(const char *name)
{
  return 0;
}

void
UART_init(int unit_num, uint32_t txd_pin, uint32_t rxd_pin, RingBuffer *ring_buffer)
{
}

uint32_t
UART_set_baudrate(int unit_num, uint32_t baudrate)
{
  return baudrate;
}

void
UART_set_flow_control(int unit_num, bool cts, bool rts)
{
}

void
UART_set_format(int unit_num, uint32_t data_bits, uint32_t stop_bits, uint8_t parity)
{
}

void
UART_set_function(uint32_t pin)
{
}

bool
UART_is_writable(int unit_num)
{
  return true;
}

void
UART_write_blocking(int unit_num, const uint8_t *src, size_t len)
{
}

bool
UART_is_readable(int unit_num)
{
  return true;
}

size_t
UART_read_nonblocking(int unit_num, uint8_t *dst, size_t maxlen)
{
  return 0;
}

void
UART_break(int unit_num, uint32_t interval)
{
}

void
UART_flush(int unit_num)
{
}

void
UART_clear_rx_buffer(int unit_num)
{
}

void
UART_clear_tx_buffer(int unit_num)
{
  // no-op
}

