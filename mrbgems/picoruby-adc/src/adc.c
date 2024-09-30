#include <stdbool.h>
#include <mrubyc.h>
#include "../include/adc.h"

#define GETIV(str)      mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define SETIV(str, val) mrbc_instance_setiv(&v[0], mrbc_str_to_symid(#str), val)

static int
pin_num(mrbc_value pin)
{
  switch (mrbc_type(pin)) {
    case MRBC_TT_INTEGER: {
      return pin.i;
    }
    case MRBC_TT_STRING: {
      return ADC_pin_num_from_char(pin.string->data);
    }
    case MRBC_TT_SYMBOL: {
      return ADC_pin_num_from_char((const uint8_t *)mrbc_symid_to_str(mrbc_symbol(pin)));
    }
    default:
      return -1;
  }
}

static void
c__init(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int input = -1;
  int pin_number = pin_num(v[1]);
  if (-1 < pin_number) {
    input = ADC_init(pin_number);
    if (input < 0) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Wrong ADC pin value");
    }
  } else {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Wrong GPIO pin value");
    return;
  }
  SET_INT_RETURN(input);
}

static void
c_read_raw(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value ivar_input = GETIV(input);
  SET_INT_RETURN(ADC_read_raw(ivar_input.i));
}

static void
c_read_voltage(mrbc_vm *vm, mrbc_value v[], int argc)
{
#ifdef MRBC_USE_FLOAT
  mrbc_value voltage = mrbc_float_value(vm, ADC_read_voltage(GETIV(input).i));
  SET_RETURN(voltage);
#else
  mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "not implemented: ADC#read_voltage");
#endif
}

void
mrbc_adc_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_ADC = mrbc_define_class(vm, "ADC", mrbc_class_object);

  mrbc_define_method(vm, mrbc_class_ADC, "_init", c__init);
  mrbc_define_method(vm, mrbc_class_ADC, "read_voltage", c_read_voltage);
  mrbc_define_method(vm, mrbc_class_ADC, "read", c_read_voltage);
  mrbc_define_method(vm, mrbc_class_ADC, "read_raw", c_read_raw);
}
