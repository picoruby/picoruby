#include <mrubyc.h>

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

void
mrbc_prk_keyboard_init(void)
{
#ifndef PRK_NO_MSC
  mrbc_class *mrbc_class_Keyboard = mrbc_define_class(0, "Keyboard", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_Keyboard, "reload_keymap",    c_Keyboard_reload_keymap);
  mrbc_define_method(0, mrbc_class_Keyboard, "suspend_keymap",   c_Keyboard_suspend_keymap);
  mrbc_define_method(0, mrbc_class_Keyboard, "resume_keymap",    c_Keyboard_resume_keymap);
#endif /* PRK_NO_MSC */
}
