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


static size_t
mrb_spi_calculate_buffer_size(mrb_state *mrb, mrb_value *args, mrb_int argc, size_t additional_bytes)
{
  size_t total_size = additional_bytes;
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
        return 0;
    }
  }
  return total_size;
}

static void
mrb_spi_fill_buffer(mrb_state *mrb, uint8_t *buffer, mrb_value *args, mrb_int argc, size_t additional_zeros)
{
  size_t pos = 0;
  for (mrb_int i = 0; i < argc; i++) {
    mrb_value arg = args[i];
    switch (mrb_type(arg)) {
      case MRB_TT_ARRAY: {
        mrb_int ary_len = RARRAY_LEN(arg);
        for (mrb_int j = 0; j < ary_len; j++) {
          mrb_value data = RARRAY_PTR(arg)[j];
          if (!mrb_fixnum_p(data)) {
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
      default:
        mrb_raise(mrb, E_ARGUMENT_ERROR, "argument must be Integer, Array or String");
        return;
    }
  }

  // Fill remaining bytes with zeros (for additional read bytes)
  for (size_t i = 0; i < additional_zeros; i++) {
    buffer[pos++] = 0;
  }
}

static mrb_int
mrb_spi_write_outputs(mrb_state *mrb, spi_unit_info_t *unit_info, mrb_value *args, mrb_int argc)
{
  size_t total_size = mrb_spi_calculate_buffer_size(mrb, args, argc, 0);
  if (total_size == 0) return -1;

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
  mrb_spi_fill_buffer(mrb, buffer, args, argc, 0);

  mrb_int res = SPI_write_blocking(unit_info, buffer, total_size);

  if (needs_free) {
    mrb_free(mrb, buffer);
  }

  return res;
}

static mrb_value
mrb_write(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  mrb_value *args;
  mrb_int argc;
  mrb_get_args(mrb, "*", &args, &argc);

  mrb_int total_bytes = mrb_spi_write_outputs(mrb, unit_info, args, argc);

  if (total_bytes < 0) {
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, "SPI write failed");
  }

  return mrb_fixnum_value(total_bytes);
}

static mrb_value
mrb_read(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  mrb_int len;
  mrb_int repeated_tx_data = 0;
  mrb_get_args(mrb, "i|i", &len, &repeated_tx_data);
  uint8_t rxdata[len];
  int ret_len = SPI_read_blocking(unit_info, rxdata, len, repeated_tx_data);
  if (-1 < ret_len) {
    return mrb_str_new(mrb, (const char *)rxdata, ret_len);
  } else {
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, "SPI read failed");
  }
}

static mrb_value
mrb_transfer(mrb_state *mrb, mrb_value self)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)mrb_data_get_ptr(mrb, self, &mrb_spi_type);
  mrb_value *args;
  mrb_int argc;
  mrb_int additional_read_bytes = 0;

  const mrb_sym kw_names[] = { MRB_SYM(additional_read_bytes) };
  mrb_value kw_values[1];
  mrb_value kw_rest = mrb_nil_value();
  mrb_kwargs kwargs = { 1, 0, kw_names, kw_values, &kw_rest };

  mrb_get_args(mrb, "*:", &args, &argc, &kwargs);

  if (!mrb_undef_p(kw_values[0])) {
    additional_read_bytes = mrb_fixnum(kw_values[0]);
  }

  size_t total_size = mrb_spi_calculate_buffer_size(mrb, args, argc, additional_read_bytes);

  // Choose buffer allocation strategy
  uint8_t stack_tx_buffer[MAX_STACK_BUFFER_SIZE];
  uint8_t stack_rx_buffer[MAX_STACK_BUFFER_SIZE];
  uint8_t *txdata, *rxdata;
  mrb_bool needs_free = false;

  if (total_size < MAX_STACK_BUFFER_SIZE) {
    txdata = stack_tx_buffer;
    rxdata = stack_rx_buffer;
  } else {
    txdata = (uint8_t *)mrb_malloc(mrb, total_size * 2);
    rxdata = txdata + total_size;
    needs_free = true;
  }

  // Fill buffer with all arguments plus additional zeros
  mrb_spi_fill_buffer(mrb, txdata, args, argc, additional_read_bytes);

  int ret_len = SPI_transfer(unit_info, txdata, rxdata, total_size);

  mrb_value result;
  if (ret_len == total_size) {
    result = mrb_str_new(mrb, (const char *)rxdata, ret_len);
  } else {
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    if (needs_free) mrb_free(mrb, txdata);
    mrb_raise(mrb, IOError, "SPI transfer failed");
  }

  if (needs_free) {
    mrb_free(mrb, txdata);
  }

  return result;
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
mrb_s_init(mrb_state *mrb, mrb_value klass)
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

  mrb_value self = mrb_obj_value(Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_spi_type, unit_info));

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
  return self;
}

void
mrb_picoruby_spi_gem_init(mrb_state* mrb)
{
  struct RClass *class_SPI = mrb_define_class_id(mrb, MRB_SYM(SPI), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_SPI, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_SPI, MRB_SYM(init), mrb_s_init, MRB_ARGS_REQ(9));
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(write), mrb_write, MRB_ARGS_REST());
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(read), mrb_read, MRB_ARGS_ARG(1,1));
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(transfer), mrb_transfer, MRB_ARGS_REST()|MRB_ARGS_KEY(1, 0));
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(sck_pin), mrb_sck_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(cipo_pin), mrb_cipo_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(copi_pin), mrb_copi_pin, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SPI, MRB_SYM(cs_pin), mrb_cs_pin, MRB_ARGS_NONE());
}

void
mrb_picoruby_spi_gem_final(mrb_state* mrb)
{
}
