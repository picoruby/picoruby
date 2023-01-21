#include <mrubyc.h>
#include "../include/i2c.h"

static void
c__write(mrb_vm *vm, mrb_value *v, int argc)
{
  uint8_t unit_num = GET_INT_ARG(1);
  uint8_t addr = GET_INT_ARG(2);
  mrbc_array value_ary = *(GET_ARY_ARG(3).array);
  int len = value_ary.n_stored;
  uint8_t src[len];
  for (int i = 0; i < len; i++) {
    src[i] = mrbc_integer(value_ary.data[i]);
  }
  SET_INT_RETURN(I2C_write_blocking(unit_num, addr, src, len, false));
}

static void
c__read(mrb_vm *vm, mrb_value *v, int argc)
{
  uint8_t unit_num = GET_INT_ARG(1);
  uint8_t addr = GET_INT_ARG(2);
  int len = GET_INT_ARG(3);
  uint8_t rxdata[len];
  int ret_len = I2C_read_blocking(unit_num, addr, rxdata, len, false);
  mrbc_value value = mrbc_string_new(vm, (const char *)rxdata, ret_len);
  SET_RETURN(value);
}

static void
c__poll(mrb_vm *vm, mrb_value *v, int argc)
{
  uint8_t unit_num = GET_INT_ARG(1);
  uint8_t addr = 0x0A;
  uint8_t rxdata[5];
  int ret;
  ret = I2C_read_blocking(unit_num, addr, rxdata, 5, false);
  if (ret != 5) {
    console_printf("error: %d\n", ret);
  } else if (0 < (rxdata[0]|rxdata[1]|rxdata[2]|rxdata[3]|rxdata[4])) {
    console_printf("%d %d %d %d %d\n", rxdata[0], rxdata[1], rxdata[2], rxdata[3], rxdata[4]);
  }
}

static void
c__init(mrb_vm *vm, mrb_value *v, int argc)
{
  uint8_t unit_num;
  if (strcmp(GET_STRING_ARG(1), "RP2040_I2C0") == 0) {
    unit_num = 0;
  } else {
    unit_num = 1;
  }
  I2C_gpio_init(
    unit_num,
    (uint32_t)GET_INT_ARG(2),
    (uint8_t)GET_INT_ARG(3),
    (uint8_t)GET_INT_ARG(4)
  );
  SET_INT_RETURN(unit_num);
}

void
mrbc_i2c_init(void)
{
  mrbc_class *mrbc_class_I2C = mrbc_define_class(0, "I2C", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_I2C, "_init", c__init);
  mrbc_define_method(0, mrbc_class_I2C, "_poll", c__poll);
  mrbc_define_method(0, mrbc_class_I2C, "_write", c__write);
}

