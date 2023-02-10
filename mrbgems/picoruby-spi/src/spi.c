#include <string.h>
#include <mrubyc.h>
#include "../include/spi.h"

static void
c__write(mrb_vm *vm, mrb_value *v, int argc)
{
  int unit_num = GET_INT_ARG(1);
  mrbc_array value_ary = *(GET_ARY_ARG(2).array);
  int len = value_ary.n_stored;
  uint8_t src[len];
  for (int i = 0; i < len; i++) {
    src[i] = mrbc_integer(value_ary.data[i]);
  }
  SET_INT_RETURN(SPI_write_blocking(unit_num, src, len));
}

static void
c__read(mrb_vm *vm, mrb_value *v, int argc)
{
  size_t len = GET_INT_ARG(2);
  uint8_t rxdata[len];
  int ret_len = SPI_read_blocking(GET_INT_ARG(1), rxdata, len, GET_INT_ARG(3));
  if (-1 < ret_len) {
    mrbc_value value = mrbc_string_new(vm, (const char *)rxdata, ret_len);
    SET_RETURN(value);
  } else {
    SET_INT_RETURN(ret_len);
  }
}

static void
c__init(mrb_vm *vm, mrb_value *v, int argc)
{
  int unit_num = SPI_unit_name_to_unit_num((const char *)GET_STRING_ARG(1));
  spi_status_t status = SPI_gpio_init(
    unit_num,
    (uint32_t)GET_INT_ARG(2), // frequency
    (int8_t) GET_INT_ARG(3),  // sck_pin
    (int8_t) GET_INT_ARG(4),  // cipo_pin
    (int8_t) GET_INT_ARG(5),  // copi_pin
    (uint8_t)GET_INT_ARG(6),  // mode
    (uint8_t)GET_INT_ARG(7),  // first_bit
    (uint8_t)GET_INT_ARG(8)   // data bits
  );
  if (status < 0) {
    char message[30];
    switch (status) {
      case ERROR_INVALID_UNIT: { strcpy(message, "Invalid SPI unit"); break; }
      case ERROR_INVALID_MODE: { strcpy(message, "Invalid SPI mode"); break; }
      case ERROR_INVALID_FIRST_BIT: { strcpy(message, "Invalid SPI firt bit"); break; }
      default: { strcpy(message, "Unknows SPI error"); }
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), message);
      return;
    }
  }
  SET_INT_RETURN(unit_num);
}

void
mrbc_spi_init(void)
{
  mrbc_class *mrbc_class_SPI = mrbc_define_class(0, "SPI", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_SPI, "_init", c__init);
  mrbc_define_method(0, mrbc_class_SPI, "_write", c__write);
  mrbc_define_method(0, mrbc_class_SPI, "_read", c__read);
}

