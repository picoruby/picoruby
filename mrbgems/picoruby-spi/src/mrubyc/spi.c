#include <mrubyc.h>

#define UNIT_INFO() \
  spi_unit_info_t *unit_info; \
  do { \
    mrbc_value unit_info_ivar = mrbc_instance_getiv(v, unit_info_symid); \
    unit_info = (spi_unit_info_t *)unit_info_ivar.string->data; \
  } while (0)

static mrbc_sym unit_info_symid;

static void
c__write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  UNIT_INFO();
  mrbc_array value_ary = *(GET_ARY_ARG(1).array);
  int len = value_ary.n_stored;
  uint8_t txdata[len];
  for (int i = 0; i < len; i++) {
    txdata[i] = mrbc_integer(value_ary.data[i]);
  }
  SET_INT_RETURN(SPI_write_blocking(unit_info, txdata, len));
}

static void
c__read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  UNIT_INFO();
  size_t len = GET_INT_ARG(1);
  uint8_t rxdata[len];
  int ret_len = SPI_read_blocking(unit_info, rxdata, len, GET_INT_ARG(2));
  if (-1 < ret_len) {
    mrbc_value value = mrbc_string_new(vm, (const char *)rxdata, ret_len);
    SET_RETURN(value);
  } else {
    SET_INT_RETURN(ret_len);
  }
}

static void
c__transfer(mrbc_vm *vm, mrbc_value *v, int argc)
{
  UNIT_INFO();
  mrbc_array value_ary = *(GET_ARY_ARG(1).array);
  int len = value_ary.n_stored;
  uint8_t txdata[len];
  uint8_t rxdata[len];
  for (int i = 0; i < len; i++) {
    txdata[i] = mrbc_integer(value_ary.data[i]);
  }
  int ret_len = SPI_transfer(unit_info, txdata, rxdata, len);
  if (ret_len == len) {
    mrbc_value value = mrbc_string_new(vm, (const char *)rxdata, ret_len);
    SET_RETURN(value);
  } else {
    SET_INT_RETURN(ret_len);
  }
}

static void
c_sck_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  UNIT_INFO();
  SET_INT_RETURN(unit_info->sck_pin);
}

static void
c_copi_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  UNIT_INFO();
  SET_INT_RETURN(unit_info->copi_pin);
}

static void
c_cipo_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  UNIT_INFO();
  SET_INT_RETURN(unit_info->cipo_pin);
}

static void
c_cs_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  UNIT_INFO();
  SET_INT_RETURN(unit_info->cs_pin);
}

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int unit_num = PICORUBY_SPI_BITBANG;
  if (strcmp((const char *)GET_STRING_ARG(1), "BITBANG") != 0) {
    unit_num = SPI_unit_name_to_unit_num((const char *)GET_STRING_ARG(1));
  }
  spi_unit_info_t unit_info = {
    .unit_num  = unit_num,
    .frequency = (uint32_t)GET_INT_ARG(2),
    .sck_pin   = (int8_t) GET_INT_ARG(3),
    .cipo_pin  = (int8_t) GET_INT_ARG(4),
    .copi_pin  = (int8_t) GET_INT_ARG(5),
    .cs_pin    = (int8_t) GET_INT_ARG(6),
    .mode      = (uint8_t)GET_INT_ARG(7),
    .first_bit = (uint8_t)GET_INT_ARG(8),
    .data_bits = (uint8_t)GET_INT_ARG(9)
  };
  mrbc_value value = mrbc_string_new(vm, (const char *)&unit_info, sizeof(spi_unit_info_t));
  unit_info_symid = mrbc_str_to_symid("_unit_info");
  mrbc_instance_setiv(v, unit_info_symid, &value);
  spi_status_t status = SPI_gpio_init(&unit_info);
  if (status < 0) {
    char message[30];
    switch (status) {
      case SPI_ERROR_INVALID_UNIT: { strcpy(message, "Invalid SPI unit"); break; }
      case SPI_ERROR_INVALID_MODE: { strcpy(message, "Invalid SPI mode"); break; }
      case SPI_ERROR_INVALID_FIRST_BIT: { strcpy(message, "Invalid SPI firt bit"); break; }
      default: { strcpy(message, "Unknows SPI error"); }
      mrbc_raise(vm, MRBC_CLASS(IOError), message);
      return;
    }
  }
  SET_INT_RETURN(unit_num);
}

void
mrbc_spi_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_SPI = mrbc_define_class(vm, "SPI", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_SPI, "_init", c__init);
  mrbc_define_method(vm, mrbc_class_SPI, "_write", c__write);
  mrbc_define_method(vm, mrbc_class_SPI, "_read", c__read);
  mrbc_define_method(vm, mrbc_class_SPI, "_transfer", c__transfer);
  mrbc_define_method(vm, mrbc_class_SPI, "sck_pin", c_sck_pin);
  mrbc_define_method(vm, mrbc_class_SPI, "cipo_pin", c_cipo_pin);
  mrbc_define_method(vm, mrbc_class_SPI, "copi_pin", c_copi_pin);
  mrbc_define_method(vm, mrbc_class_SPI, "cs_pin", c_cs_pin);
}
