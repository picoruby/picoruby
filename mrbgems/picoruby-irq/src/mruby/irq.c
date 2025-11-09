#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/hash.h>
#include <mruby/array.h>

#include "../../include/irq.h"

/*
 * IRQ.register_gpio(pin, event_type, opts)
 */
static mrb_value
mrb_s_register_gpio(mrb_state *mrb, mrb_value self)
{
  mrb_int pin;
  mrb_int event_type;
  mrb_value opts;
  mrb_get_args(mrb, "iiH", &pin, &event_type, &opts);

  /* Extract debounce from opts hash */
  uint32_t debounce_ms = 0;
  if (!mrb_nil_p(opts)) {
    mrb_value debounce_val = mrb_hash_get(mrb, opts, mrb_symbol_value(MRB_SYM(debounce)));
    if (!mrb_nil_p(debounce_val)) {
      debounce_ms = (uint32_t)mrb_integer(debounce_val);
    }
  }

  int irq_id = IRQ_register_gpio(pin, event_type, debounce_ms);

  if (irq_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to register GPIO IRQ");
  }

  return mrb_fixnum_value(irq_id);
}

/*
 * IRQ.unregister_gpio(id)
 */
static mrb_value
mrb_s_unregister_gpio(mrb_state *mrb, mrb_value self)
{
  mrb_int irq_id;
  mrb_get_args(mrb, "i", &irq_id);

  bool prev_state = IRQ_unregister_gpio(irq_id);
  return mrb_bool_value(prev_state);
}

/*
 * IRQ.peek_event()
 */
static mrb_value
mrb_s_peek_event(mrb_state *mrb, mrb_value self)
{
  int irq_id, event_type;
  bool has_event = IRQ_peek_event(&irq_id, &event_type);

  mrb_value result = mrb_ary_new_capa(mrb, 2);
  if (has_event) {
    mrb_ary_push(mrb, result, mrb_fixnum_value(irq_id));
  } else {
    mrb_ary_push(mrb, result, mrb_nil_value());
  }
  mrb_ary_push(mrb, result, mrb_fixnum_value(event_type));

  return result;
}

void
mrb_picoruby_irq_gem_init(mrb_state *mrb)
{
  struct RClass *module_IRQ = mrb_define_module(mrb, "IRQ");

  /* Define singleton methods */
  mrb_define_module_function_id(mrb, module_IRQ, MRB_SYM(register_gpio), mrb_s_register_gpio, MRB_ARGS_REQ(3));
  mrb_define_module_function_id(mrb, module_IRQ, MRB_SYM(unregister_gpio), mrb_s_unregister_gpio, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, module_IRQ, MRB_SYM(peek_event), mrb_s_peek_event, MRB_ARGS_NONE());

  IRQ_init(); // Initialize the IRQ system
}

void
mrb_picoruby_irq_gem_final(mrb_state *mrb)
{
  /* Cleanup if needed */
}
