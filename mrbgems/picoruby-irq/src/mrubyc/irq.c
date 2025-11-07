#include <mrubyc.h>
#include "../../include/irq.h"

/*
 * IRQ.register_gpio(pin, event_type, opts)
 */
static void
c_register_gpio(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  if (v[1].tt != MRBC_TT_INTEGER || v[2].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "pin and event_type must be integers");
    return;
  }

  int pin = v[1].i;
  int event_type = v[2].i;

  /* Extract debounce from opts hash */
  uint32_t debounce_ms = 0;
  if (argc >= 3 && v[3].tt == MRBC_TT_HASH) {
    mrbc_value debounce_key = mrbc_symbol_value(mrbc_str_to_symid("debounce"));
    mrbc_value debounce_val = mrbc_hash_get(&v[3], &debounce_key);
    if (debounce_val.tt == MRBC_TT_INTEGER) {
      debounce_ms = (uint32_t)debounce_val.i;
    }
  }

  int irq_id = IRQ_register_gpio(pin, event_type, debounce_ms);

  if (irq_id < 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Failed to register GPIO IRQ");
    return;
  }

  SET_INT_RETURN(irq_id);
}

/*
 * IRQ.unregister_gpio(id)
 */
static void
c_unregister_gpio(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "irq_id must be integer");
    return;
  }

  int irq_id = v[1].i;
  bool prev_state = IRQ_unregister_gpio(irq_id);

  if (prev_state) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/*
 * IRQ.peek_event()
 */
static void
c_peek_event(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int irq_id, event_type;
  bool has_event = IRQ_peek_event(&irq_id, &event_type);

  mrbc_value result = mrbc_array_new(vm, 2);

  if (has_event) {
    mrbc_array_set(&result, 0, &mrbc_integer_value(irq_id));
  } else {
    mrbc_array_set(&result, 0, &mrbc_nil_value());
  }
  mrbc_array_set(&result, 1, &mrbc_integer_value(event_type));

  SET_RETURN(result);
}

#define SET_MODULE_CONST(mod, cst) \
  mrbc_set_const(mod, mrbc_str_to_symid(#cst), &mrbc_integer_value(cst))

void
mrbc_irq_init(mrbc_vm *vm)
{
  mrbc_class *module_IRQ = mrbc_define_module(vm, "IRQ");

  /* Define module methods */
  mrbc_define_method(vm, module_IRQ, "register_gpio", c_register_gpio);
  mrbc_define_method(vm, module_IRQ, "unregister_gpio", c_unregister_gpio);  
  mrbc_define_method(vm, module_IRQ, "peek_event", c_peek_event);

  /* Initialize the IRQ system */
  IRQ_init();
}
