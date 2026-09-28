#include <mruby.h>
#include <mruby/presym.h>

static mrb_value
mrb_pulse_counter__init(mrb_state *mrb, mrb_value self)
{
  mrb_int pin_a, pin_b, glitch_ns;
  mrb_bool pull_up;
  mrb_get_args(mrb, "iiib", &pin_a, &pin_b, &glitch_ns, &pull_up);

  int ret = PulseCounter_init((uint32_t)pin_a, (uint32_t)pin_b, (uint32_t)glitch_ns, pull_up);
  return mrb_fixnum_value(ret);
}

static mrb_value
mrb_pulse_counter__count(mrb_state *mrb, mrb_value self)
{
  mrb_int unit_id;
  mrb_get_args(mrb, "i", &unit_id);

  int32_t count;
  if (PulseCounter_get_count((int)unit_id, &count) != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "PulseCounter: failed to get count");
  }
  return mrb_fixnum_value(count);
}

static mrb_value
mrb_pulse_counter__clear(mrb_state *mrb, mrb_value self)
{
  mrb_int unit_id;
  mrb_get_args(mrb, "i", &unit_id);
  return mrb_fixnum_value(PulseCounter_clear((int)unit_id));
}

void
mrb_picoruby_pulse_counter_gem_init(mrb_state* mrb)
{
  struct RClass *class_PulseCounter = mrb_define_class_id(mrb, MRB_SYM(PulseCounter), mrb->object_class);

  mrb_define_method_id(mrb, class_PulseCounter, MRB_SYM(_init), mrb_pulse_counter__init, MRB_ARGS_REQ(4));
  mrb_define_method_id(mrb, class_PulseCounter, MRB_SYM(_count), mrb_pulse_counter__count, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_PulseCounter, MRB_SYM(_clear), mrb_pulse_counter__clear, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_pulse_counter_gem_final(mrb_state* mrb)
{
}
