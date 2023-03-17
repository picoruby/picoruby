#include <string.h>
#include <mrubyc.h>
#include "../include/i2c.h"

static uint32_t duration_1byte = 0;

static void
c__write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_array value_ary = *(GET_ARY_ARG(3).array);
  int len = value_ary.n_stored;
  uint8_t src[len];
  for (int i = 0; i < len; i++) {
    src[i] = mrbc_integer(value_ary.data[i]);
  }
  SET_INT_RETURN(
    I2C_write_timeout_us((
      uint8_t)GET_INT_ARG(1),
      (uint8_t)GET_INT_ARG(2),
      src,
      len,
      (bool)(GET_ARG(4).tt == MRBC_TT_TRUE),
      duration_1byte * len
    )
  );
}

static void
c__read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int len = GET_INT_ARG(3);
  uint8_t rxdata[len];
  int ret = I2C_read_timeout_us(
              (uint8_t)GET_INT_ARG(1),
              (uint8_t)GET_INT_ARG(2),
              rxdata,
              len,
              false,
              duration_1byte * len
            );
  if (0 < ret) {
    mrbc_value value = mrbc_string_new(vm, (const char *)rxdata, ret);
    SET_RETURN(value);
  } else {
    SET_INT_RETURN(ret);
  }
}

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int unit_num = I2C_unit_name_to_unit_num((const char *)GET_STRING_ARG(1));
  uint32_t frequency = (uint32_t)GET_INT_ARG(2);
  duration_1byte = 1000000 / frequency * 8 * 4;
  i2c_status_t status = I2C_gpio_init(unit_num, frequency, (uint8_t)GET_INT_ARG(3), (uint8_t)GET_INT_ARG(4));
  if (status < 0) {
    char message[30];
    switch (status) {
      case ERROR_INVALID_UNIT: { strcpy(message, "Invalid I2C unit"); break; }
      default: { strcpy(message, "Unknows I2C error"); }
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), message);
      return;
    }
  }
  SET_INT_RETURN(unit_num);
}

void
mrbc_i2c_init(void)
{
  mrbc_class *mrbc_class_I2C = mrbc_define_class(0, "I2C", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_I2C, "_init", c__init);
  mrbc_define_method(0, mrbc_class_I2C, "_write", c__write);
  mrbc_define_method(0, mrbc_class_I2C, "_read", c__read);
}

