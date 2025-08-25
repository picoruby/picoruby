#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/array.h>
#include <mruby/string.h>

#define MAX_STACK_BUFFER_SIZE 256

static struct RClass *IOError;

static mrb_int
mrb_i2c_write_outputs(mrb_state *mrb, mrb_int unit_num, mrb_int i2c_adrs_7, mrb_value *args, mrb_int argc, mrb_int timeout_ms, bool nostop)
{
  // Calculate total buffer size needed
  size_t total_size = 0;
  for (mrb_int i = 0; i < argc; i++) {
    mrb_value arg = args[i];
    switch (mrb_type(arg)) {
      case MRB_TT_ARRAY:
        total_size += RARRAY_LEN(arg);
        break;
      case MRB_TT_FIXNUM:
        total_size += 1;
        break;
      case MRB_TT_STRING:
        total_size += RSTRING_LEN(arg);
        break;
      default:
        mrb_raise(mrb, E_TYPE_ERROR, "argument must be Integer, Array or String");
    }
  }

  // Choose buffer allocation strategy
  uint8_t stack_buffer[MAX_STACK_BUFFER_SIZE];
  uint8_t *buffer;
  mrb_bool needs_free = false;

  if (total_size < MAX_STACK_BUFFER_SIZE) {
    buffer = stack_buffer;
  } else {
    buffer = (uint8_t *)mrb_malloc(mrb, total_size);
    needs_free = true;
  }

  // Fill buffer with all arguments
  size_t pos = 0;
  for (mrb_int i = 0; i < argc; i++) {
    mrb_value arg = args[i];
    switch (mrb_type(arg)) {
      case MRB_TT_ARRAY: {
        mrb_int ary_len = RARRAY_LEN(arg);
        for (mrb_int j = 0; j < ary_len; j++) {
          mrb_value data = RARRAY_PTR(arg)[j];
          if (!mrb_fixnum_p(data)) {
            if (needs_free) mrb_free(mrb, buffer);
            mrb_raise(mrb, E_TYPE_ERROR, "array element must be Fixnum");
          }
          buffer[pos++] = (uint8_t)mrb_fixnum(data);
        }
        break;
      }
      case MRB_TT_FIXNUM: {
        buffer[pos++] = (uint8_t)mrb_fixnum(arg);
        break;
      }
      case MRB_TT_STRING: {
        const char *str = RSTRING_PTR(arg);
        mrb_int str_len = RSTRING_LEN(arg);
        memcpy(&buffer[pos], str, str_len);
        pos += str_len;
        break;
      }
      default: {
        mrb_raise(mrb, E_ARGUMENT_ERROR, "argument must be Inteter, Array[Integer], or String");
      }
    }
  }

  // Send as single I2C transaction
  mrb_int res = I2C_write_timeout_us(
      (uint8_t)unit_num,
      (uint8_t)i2c_adrs_7,
      buffer,
      total_size,
      nostop,
      (uint32_t)timeout_ms * 1000
    );

  if (needs_free) {
    mrb_free(mrb, buffer);
  }

  return res;
}

static mrb_value
mrb_i2c_write(mrb_state *mrb, mrb_value self)
{
  mrb_value *args;
  mrb_int argc;
  mrb_int i2c_adrs_7;
  mrb_int timeout_ms;
  const mrb_sym kw_names[] = { MRB_SYM(timeout) };
  mrb_value kw_values[1];
  mrb_value kw_rest = mrb_nil_value();
  mrb_kwargs kwargs = { 1, 0, kw_names, kw_values, &kw_rest };

  mrb_get_args(mrb, "i*:", &i2c_adrs_7, &args, &argc, &kwargs);

  if (mrb_undef_p(kw_values[0])) {
    timeout_ms = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(timeout)));
  } else {
    timeout_ms = mrb_fixnum(kw_values[0]);
  }

  mrb_int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  mrb_int total_bytes = mrb_i2c_write_outputs(mrb, unit_num, i2c_adrs_7, args, argc, timeout_ms, false);

  if (total_bytes < 0) {
    mrb_raise(mrb, IOError, "I2C write failed");
  }

  return mrb_fixnum_value(total_bytes);
}

static mrb_value
mrb_i2c_read(mrb_state *mrb, mrb_value self)
{
  mrb_value *args;
  mrb_int argc;
  mrb_int i2c_adrs_7, len;
  mrb_int timeout_ms;

  mrb_int kw_num = 1;
  mrb_int kw_req = 0;
  mrb_sym kw_names[] = { MRB_SYM(timeout) };
  mrb_value kw_values[kw_num];
  mrb_kwargs kwargs = { kw_num, kw_req, kw_names, kw_values, NULL };
  mrb_get_args(mrb, "ii*:", &i2c_adrs_7, &len, &args, &argc, &kwargs);

  mrb_int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));

  if (mrb_undef_p(kw_values[0])) {
    timeout_ms = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(timeout)));
  } else {
    timeout_ms = mrb_fixnum(kw_values[0]);
  }

  if (0 < argc) {
    // it doesn't issue STOP condition
    mrb_int res = mrb_i2c_write_outputs(mrb, unit_num, i2c_adrs_7, args, argc, timeout_ms, true);
    if (res < 0) {
      mrb_raise(mrb, IOError, "I2C write (for read) failed");
    }
  }

  // Read data
  uint8_t rxdata[len];
  int ret = I2C_read_timeout_us(
              (uint8_t)unit_num,
              (uint8_t)i2c_adrs_7,
              rxdata,
              (size_t)len,
              false,
              (uint32_t)timeout_ms * 1000
            );
  if (0 < ret) {
    return mrb_str_new(mrb, (const char *)rxdata, ret);
  } else {
    mrb_raise(mrb, IOError, "I2C read failed");
  }
}

static mrb_value
mrb_i2c__init(mrb_state *mrb, mrb_value self)
{
  const char *unit;
  mrb_int frequency, sda_pin, scl_pin;
  mrb_get_args(mrb, "ziii", &unit, &frequency, &sda_pin, &scl_pin);

  int unit_num = I2C_unit_name_to_unit_num(unit);
  i2c_status_t status = I2C_gpio_init(unit_num, (uint32_t)frequency, (uint8_t)sda_pin, (uint8_t)scl_pin);
  if (status < 0) {
    const char *message;
    switch (status) {
      case I2C_ERROR_INVALID_UNIT: message = "Invalid I2C unit"; break;
      default: message = "Unknow I2C error"; break;
    }
    mrb_raise(mrb, IOError, message);
  }
  return mrb_fixnum_value(unit_num);
}

void
mrb_picoruby_i2c_gem_init(mrb_state* mrb)
{
  IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
  struct RClass *class_I2C = mrb_define_class_id(mrb, MRB_SYM(I2C), mrb->object_class);

  mrb_define_method_id(mrb, class_I2C, MRB_SYM(_init), mrb_i2c__init, MRB_ARGS_REQ(4));
  mrb_define_method_id(mrb, class_I2C, MRB_SYM(write), mrb_i2c_write, MRB_ARGS_REQ(1)|MRB_ARGS_REST()|MRB_ARGS_KEY(1, 0));
  mrb_define_method_id(mrb, class_I2C, MRB_SYM(read), mrb_i2c_read, MRB_ARGS_REQ(2)|MRB_ARGS_REST()|MRB_ARGS_KEY(1, 0));
}

void
mrb_picoruby_i2c_gem_final(mrb_state* mrb)
{
}
