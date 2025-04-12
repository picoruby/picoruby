#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/string.h>

static int
pin_num(mrb_state *mrb)
{
  mrb_value pin;
  mrb_get_args(mrb, "o", &pin);
  int pin_number;
  switch (mrb_type(pin)) {
    case MRB_TT_INTEGER: {
      pin_number = mrb_fixnum(pin);
      break;
    }
    case MRB_TT_STRING: {
      pin_number = ADC_pin_num_from_char((const uint8_t *)RSTRING_PTR(pin));
      break;
    }
    case MRB_TT_SYMBOL: {
      pin_number = ADC_pin_num_from_char((const uint8_t *)mrb_sym_name(mrb, mrb_symbol(pin)));
      break;
    }
    default:
      pin_number = -1;
  }
  if (pin_number < 0) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Wrong ADC pin name: %v", pin);
  }
  return pin_number;
}

static mrb_value
mrb_adc__init(mrb_state *mrb, mrb_value self)
{
  int pin_number = pin_num(mrb);
  int input = ADC_init(pin_number);
  if (input < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Wrong ADC pin value");
  }
  return mrb_fixnum_value(input);
}

static mrb_value
mrb_read_raw(mrb_state *mrb, mrb_value self)
{
  mrb_value ivar_input = mrb_iv_get(mrb, self, MRB_IVSYM(input));
  return mrb_fixnum_value(ADC_read_raw(mrb_fixnum(ivar_input)));
}

static mrb_value
mrb_read_voltage(mrb_state *mrb, mrb_value self)
{
#ifndef PICORB_NO_FLOAT
  mrb_value ivar_input = mrb_iv_get(mrb, self, MRB_IVSYM(input));
  return mrb_float_value(mrb, ADC_read_voltage(mrb_fixnum(ivar_input)));
#else
  mrb_notimplement(mrb);
  return mrb_nil_value();
#endif
}

void
mrb_picoruby_adc_gem_init(mrb_state* mrb)
{
  struct RClass *class_ADC = mrb_define_class_id(mrb, MRB_SYM(ADC), mrb->object_class);

  mrb_define_method_id(mrb, class_ADC, MRB_SYM(_init), mrb_adc__init, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_ADC, MRB_SYM(read_voltage), mrb_read_voltage, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_ADC, MRB_SYM(read), mrb_read_voltage, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_ADC, MRB_SYM(read_raw), mrb_read_raw, MRB_ARGS_NONE());
}

void
mrb_picoruby_adc_gem_final(mrb_state* mrb)
{
}
