#include <stdbool.h>
#include <mrubyc.h>
#include "../include/uart.h"

void
mrbc_uart_init(void)
{
  mrbc_class *mrbc_class_UART = mrbc_define_class(0, "UART", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_UART, "_init", c__init);
  mrbc_define_method(0, mrbc_class_UART, "read", c_read);
}
