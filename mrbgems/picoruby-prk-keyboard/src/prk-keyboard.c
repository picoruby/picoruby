#include <mrubyc.h>
#include "../include/prk-keyboard.h"

#ifndef PRK_NO_MSC
mrbc_tcb *tcb_keymap;

static void
c_Keyboard_resume_keymap(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_resume_task(tcb_keymap);
}

static void
c_Keyboard_suspend_keymap(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_suspend_task(tcb_keymap);
}

/*
 * TODO: The current implemantation is in prk_firmware/src/main.c
 *       Move that to this gem along with taking advantage of File and Dir class
 */
mrbc_tcb* create_keymap_task(mrbc_tcb *tcb);

static void
c_Keyboard_reload_keymap(mrbc_vm *vm, mrbc_value *v, int argc)
{
  tcb_keymap = create_keymap_task(tcb_keymap);
}
#endif /* PRK_NO_MSC */


/*
 * Software UART
 */

static void
c_uart_anchor_init(mrb_vm *vm, mrb_value *v, int argc)
{
  Keyboard_uart_anchor_init(GET_INT_ARG(1));
}

static void
c_uart_partner_init(mrb_vm *vm, mrb_value *v, int argc)
{
  Keyboard_uart_partner_init(GET_INT_ARG(1));
}

static void
c_uart_anchor(mrb_vm *vm, mrb_value *v, int argc)
{
  uint32_t data;
  data = Keyboard_mutual_anchor_put8_get24_nonblocking((uint8_t)GET_INT_ARG(1));
  SET_INT_RETURN(data);
}

static uint32_t buffer = 0;
static int      buffer_index = 0;

static void
c_uart_partner_push8(mrb_vm *vm, mrb_value *v, int argc)
{
  int keycode = GET_INT_ARG(1);
  if (buffer_index > 2) return;
  buffer |= keycode << (buffer_index * 8);
  buffer_index++;
}

static void
c_uart_partner(mrb_vm *vm, mrb_value *v, int argc)
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

#ifndef PRK_NO_MSC
  mrbc_define_method(0, mrbc_class_Keyboard, "reload_keymap",    c_Keyboard_reload_keymap);
  mrbc_define_method(0, mrbc_class_Keyboard, "suspend_keymap",   c_Keyboard_suspend_keymap);
  mrbc_define_method(0, mrbc_class_Keyboard, "resume_keymap",    c_Keyboard_resume_keymap);
#endif /* PRK_NO_MSC */

  mrbc_define_method(0, mrbc_class_Keyboard, "uart_anchor_init",   c_uart_anchor_init);
  mrbc_define_method(0, mrbc_class_Keyboard, "uart_partner_init",  c_uart_partner_init);
  mrbc_define_method(0, mrbc_class_Keyboard, "uart_anchor",        c_uart_anchor);
  mrbc_define_method(0, mrbc_class_Keyboard, "uart_partner",       c_uart_partner);
  mrbc_define_method(0, mrbc_class_Keyboard, "uart_partner_push8", c_uart_partner_push8);
}
