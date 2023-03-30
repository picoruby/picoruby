#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hardware/uart.h"

#include "../../include/uart.h"

#define UNIT_SELECT() \
  do { \
    switch (unit_num) { \
      case PICORUBY_UART_RP2040_UART0: { unit = uart0; break; } \
      case PICORUBY_UART_RP2040_UART1: { unit = uart1; break; } \
      default: { return ERROR_INVALID_UNIT; } \
    } \
  } while (0)

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
