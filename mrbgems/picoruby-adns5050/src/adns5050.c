#include <stdbool.h>
#include <mrubyc.h>
#include "../include/adns5050.h"

static void
c__init(mrbc_vm *vm, mrbc_value v[], int argc)
{
  ADNS5050_init(GET_INT_ARG(1), GET_INT_ARG(2), GET_INT_ARG(3));
  SET_INT_RETURN(0);
}

static void
c_read(mrbc_vm *vm, mrbc_value v[], int argc)
{
  uint8_t len = GET_INT_ARG(1);
  uint8_t data[len];
  for (int i = 0; i < len; i++) {
    data[i] = ADNS5050_read8(i);
  }
  mrbc_value ret = mrbc_string_new(vm, (char *)data, len);
  SET_RETURN(ret);
}

static void
c_write(mrbc_vm *vm, mrbc_value v[], int argc)
{
  ADNS5050_write8(GET_INT_ARG(1), GET_INT_ARG(2));
  SET_INT_RETURN(1);
}

void
mrbc_adns5050_init(void)
{
  mrbc_class *mrbc_class_ADNS5050 = mrbc_define_class(0, "ADNS5050", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_ADNS5050, "_init", c__init);
  mrbc_define_method(0, mrbc_class_ADNS5050, "read", c_read);
  mrbc_define_method(0, mrbc_class_ADNS5050, "write", c_write);
}

