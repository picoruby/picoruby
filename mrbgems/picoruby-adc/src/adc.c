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
c_read(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value ivar_input = GETIV(input);
  uint16_t result = ADC_read(ivar_input.i);
  mrbc_value ivar_min = GETIV(min);
  if (ivar_min.tt == MRBC_TT_NIL) {
    ivar_min.tt = MRBC_TT_INTEGER;
    ivar_min.i = 0;
  }
  if (result < ivar_min.i) {
    ivar_min.i = result;
    SETIV(min, &ivar_min);
  }
  mrbc_value ivar_max = GETIV(max);
  if (ivar_max.tt == MRBC_TT_NIL) {
    ivar_max.tt = MRBC_TT_INTEGER;
    ivar_max.i = 0;
  }
  if (ivar_max.i < result) {
    ivar_max.i = result;
    SETIV(max, &ivar_max);
  }
  SET_INT_RETURN(result);
}

void
mrbc_adc_init(void)
{
  mrbc_class *mrbc_class_ADC = mrbc_define_class(0, "ADC", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_ADC, "_init", c__init);
  mrbc_define_method(0, mrbc_class_ADC, "read", c_read);
}
