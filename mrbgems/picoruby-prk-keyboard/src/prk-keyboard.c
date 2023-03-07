#include <mrubyc.h>
#include "../include/prk-keyboard.h"

/*
 * Software UART
 */

static uint32_t buffer = 0;
static int      buffer_index = 0;

static void
c_uart_anchor_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Keyboard_uart_anchor_init(GET_INT_ARG(1));
}

static void
c_uart_partner_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Keyboard_uart_partner_init(GET_INT_ARG(1));
}

static void
c_uart_partner_push8(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int keycode = GET_INT_ARG(1);
  if (buffer_index > 2) return;
  buffer |= keycode << (buffer_index * 8);
  buffer_index++;
}

static void
c_uart_partner(mrbc_vm *vm, mrbc_value *v, int argc)
{
  switch (buffer_index) {
    case 0: buffer  = NIL; break;
    case 1: buffer |= 0xFFFF00; break;
    case 2: buffer |= 0xFF0000; break;
  }
  uint8_t data;
  data = Keyboard_mutual_partner_get8_put24_blocking(buffer);
  SET_INT_RETURN(data);
  buffer = 0;
  buffer_index = 0;
}


void
mrbc_prk_keyboard_init(void)
{
  mrbc_class *mrbc_class_Keyboard = mrbc_define_class(0, "Keyboard", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_Keyboard, "uart_anchor_init",   c_uart_anchor_init);
  mrbc_define_method(0, mrbc_class_Keyboard, "uart_partner_init",  c_uart_partner_init);
  mrbc_define_method(0, mrbc_class_Keyboard, "uart_partner",       c_uart_partner);
  mrbc_define_method(0, mrbc_class_Keyboard, "uart_partner_push8", c_uart_partner_push8);

  Keyboard_init_sub(mrbc_class_Keyboard);
}
