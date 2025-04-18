#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/array.h>

static mrb_value
mrb__write(mrb_state *mrb, mrb_value self)
{
  mrb_int unit_num, i2c_adrs_7, ary_len;
  mrb_value outputs;
  mrb_bool nostop;
  mrb_get_args(mrb, "iiAb", &unit_num, &i2c_adrs_7,  &outputs, &nostop);

  mrb_value duration_1byte = mrb_iv_get(mrb, self, MRB_IVSYM(duration_1byte));

  ary_len = RARRAY_LEN(outputs);
  uint8_t src[ary_len];
  for (int i = 0; i < ary_len; i++) {
    mrb_value data = RARRAY_PTR(outputs)[i];
    if (!mrb_fixnum_p(data)) {
      mrb_raise(mrb, E_TYPE_ERROR, "data must be Fixnum");
    }
    src[i] = (uint8_t)mrb_fixnum(data);
  }
  mrb_int res = I2C_write_timeout_us(
      (uint8_t)unit_num,
      (uint8_t)i2c_adrs_7,
      src,
      ary_len,
      nostop,
      mrb_fixnum(duration_1byte) * ary_len
    );
  return mrb_fixnum_value(res);
}

static mrb_value
mrb__read(mrb_state *mrb, mrb_value self)
{
  mrb_int i2c_adrs_7, len;
  mrb_value outputs;
  mrb_get_args(mrb, "iiA", &i2c_adrs_7, &len, &outputs);

  mrb_value duration_1byte = mrb_iv_get(mrb, self, MRB_IVSYM(duration_1byte));

  mrb_int ary_len = RARRAY_LEN(outputs);
  uint8_t rxdata[ary_len];
  int ret = I2C_read_timeout_us(
              (uint8_t)i2c_adrs_7,
              (uint8_t)len,
              rxdata,
              ary_len,
              false,
              mrb_fixnum(duration_1byte) * ary_len
            );
  if (0 < ret) {
    return mrb_str_new(mrb, (const char *)rxdata, ret);
  } else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb__init(mrb_state *mrb, mrb_value self)
{
  const char *unit;
  mrb_int frequency, sda_pin, scl_pin;
  mrb_get_args(mrb, "ziii", &unit, &frequency, &sda_pin, &scl_pin);

  int unit_num = I2C_unit_name_to_unit_num(unit);
  mrb_iv_set(mrb, self, MRB_IVSYM(duration_1byte), mrb_fixnum_value(1000000 / frequency * 8 * 4));
  i2c_status_t status = I2C_gpio_init(unit_num, (uint32_t)frequency, (uint8_t)sda_pin, (uint8_t)scl_pin);
  if (status < 0) {
    const char *message;
    switch (status) {
      case ERROR_INVALID_UNIT: message = "Invalid I2C unit"; break;
      default: message = "Unknows I2C error"; break;
    }
    struct RClass *IOError = mrb_define_class_id(mrb, MRB_SYM(IOError), E_STANDARD_ERROR);
    mrb_raise(mrb, IOError, message);
  }
  return mrb_fixnum_value(unit_num);
}

void
mrb_picoruby_i2c_gem_init(mrb_state* mrb)
{
  struct RClass *class_I2C = mrb_define_class_id(mrb, MRB_SYM(I2C), mrb->object_class);

  mrb_define_method_id(mrb, class_I2C, MRB_SYM(_init), mrb__init, MRB_ARGS_REQ(4));
  mrb_define_method_id(mrb, class_I2C, MRB_SYM(_write), mrb__write, MRB_ARGS_REQ(4));
  mrb_define_method_id(mrb, class_I2C, MRB_SYM(_read), mrb__read, MRB_ARGS_REQ(3));
}

void
mrb_picoruby_i2c_gem_final(mrb_state* mrb)
{
}
