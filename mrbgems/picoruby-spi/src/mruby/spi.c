#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/array.h>
#include <mruby/data.h>
#include <mruby/class.h>

static void
mrb_spi_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_spi_type = {
  "SPI", mrb_spi_free
};


static mrb_value
mrb__write(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  mrb_value value_ary;
  mrb_get_args(mrb, "A", &value_ary);
  mrb_int len = RARRAY_LEN(value_ary);
  uint8_t txdata[len];
  for (int i = 0; i < len; i++) {
    mrb_value value = mrb_ary_ref(mrb, value_ary, i);
    if (mrb_fixnum_p(value)) {
      txdata[i] = mrb_fixnum(value);
    } else {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "value must be Fixnum");
    }
  }
  mrb_int res = SPI_write_blocking(unit_info, txdata, len);
  return mrb_fixnum_value(res);
}

static mrb_value
mrb__read(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  mrb_int len, repeated_tx_data;
  mrb_get_args(mrb, "ii", &len, &repeated_tx_data);
  uint8_t rxdata[len];
  int ret_len = SPI_read_blocking(unit_info, rxdata, len, repeated_tx_data);
  if (-1 < ret_len) {
    return mrb_str_new(mrb, (const char *)rxdata, ret_len);
  } else {
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, "SPI read error");
  }
}

static mrb_value
mrb__transfer(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  mrb_value value_ary;
  mrb_get_args(mrb, "A", &value_ary);
  mrb_int len = RARRAY_LEN(value_ary);
  uint8_t txdata[len];
  uint8_t rxdata[len];
  for (int i = 0; i < len; i++) {
    mrb_value value = mrb_ary_ref(mrb, value_ary, i);
    if (!mrb_fixnum_p(value)) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "value must be Fixnum");
    }
    txdata[i] = mrb_fixnum(value);
  }
  int ret_len = SPI_transfer(unit_info, txdata, rxdata, len);
  if (ret_len == len) {
    return mrb_str_new(mrb, (const char *)rxdata, ret_len);
  }
  struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
  mrb_raise(mrb, IOError, "SPI transfer error");
}

static mrb_value
mrb_sck_pin(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  return mrb_fixnum_value(unit_info->sck_pin);
}

static mrb_value
mrb_copi_pin(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  return mrb_fixnum_value(unit_info->copi_pin);
}

static mrb_value
mrb_cipo_pin(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  return mrb_fixnum_value(unit_info->cipo_pin);
}

static mrb_value
mrb_cs_pin(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  return mrb_fixnum_value(unit_info->cs_pin);
}

static mrb_value
mrb__init(mrb_state *mrb, mrb_value self)
{
  int unit_num = PICORUBY_SPI_BITBANG;
  const char *unit_name;
  mrb_int frequency, sck_pin, cipo_pin, copi_pin, cs_pin, mode, first_bit, data_bits;
  mrb_get_args(mrb, "ziiiiiiii", &unit_name, &frequency, &sck_pin, &cipo_pin, &copi_pin, &cs_pin, &mode, &first_bit, &data_bits);
  if (strcmp(unit_name, "BITBANG") != 0) {
    unit_num = SPI_unit_name_to_unit_num(unit_name);
  }
  if (unit_num < 0) {
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, "Invalid SPI unit name");
  }
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_malloc(mrb, sizeof(spi_unit_info_t));
  unit_info->unit_num  = unit_num;
  unit_info->frequency = (uint32_t)frequency;
  unit_info->sck_pin   = (int8_t) sck_pin;
  unit_info->cipo_pin  = (int8_t) cipo_pin;
  unit_info->copi_pin  = (int8_t) copi_pin;
  unit_info->cs_pin    = (int8_t) cs_pin;
  unit_info->mode      = (uint8_t)mode;
  unit_info->first_bit = (uint8_t)first_bit;
  unit_info->data_bits = (uint8_t)data_bits;

  DATA_PTR(self) = unit_info;
  DATA_TYPE(self) = &mrb_spi_type;
  
  spi_status_t status = SPI_gpio_init(unit_info);
  if (status < 0) {
    const char *message;
    switch (status) {
      case SPI_ERROR_INVALID_UNIT: message = "Invalid SPI unit"; break;
      case SPI_ERROR_INVALID_MODE: message = "Invalid SPI mode"; break;
      case SPI_ERROR_INVALID_FIRST_BIT: message = "Invalid SPI first bit"; break;
      default: message = "Unknown SPI error"; break;
    }
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, message);
  }
  return mrb_fixnum_value(unit_num);
}

void
mrb_picoruby_spi_gem_init(mrb_state* mrb)
{
  struct RClass *class_SPI = mrb_define_class_id(mrb, MRB_SYM(SPI), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_SPI, MRB_TT_CDATA);

  mrb_define_method_id(mrb, class_SPI, MRB_SYM(_init), mrb__init, MRB_ARGS_REQ(9));
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(_write), mrb__write, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(_read), mrb__read, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(_transfer), mrb__transfer, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(sck_pin), mrb_sck_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(cipo_pin), mrb_cipo_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(copi_pin), mrb_copi_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(cs_pin), mrb_cs_pin, MRB_ARGS_NONE());
}

void
mrb_picoruby_spi_gem_final(mrb_state* mrb)
{
}
