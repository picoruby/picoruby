#include <mrubyc.h>

static size_t
mrbc_spi_calculate_buffer_size(mrbc_vm *vm, mrbc_value *args, int argc, size_t additional_bytes)
{
  size_t total_size = additional_bytes;
  for (int i = 0; i < argc; i++) {
    mrbc_value arg = args[i];
    switch (arg.tt) {
      case MRBC_TT_ARRAY:
        total_size += arg.array->n_stored;
        break;
      case MRBC_TT_INTEGER:
        total_size += 1;
        break;
      case MRBC_TT_STRING:
        total_size += arg.string->size;
        break;
      default:
        mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer, Array or String");
        return 0;
    }
  }
  return total_size;
}

static void
mrbc_spi_fill_buffer(mrbc_vm *vm, uint8_t *buffer, mrbc_value *args, int argc, size_t additional_zeros)
{
  size_t pos = 0;
  for (int i = 0; i < argc; i++) {
    mrbc_value arg = args[i];
    switch (arg.tt) {
      case MRBC_TT_ARRAY: {
        mrbc_array *ary = arg.array;
        mrbc_int_t ary_len = ary->n_stored;
        for (mrbc_int_t j = 0; j < ary_len; j++) {
          if (ary->data[j].tt != MRBC_TT_INTEGER) {
            mrbc_raise(vm, MRBC_CLASS(TypeError), "array element must be Integer");
            return;
          }
          buffer[pos++] = (uint8_t)mrbc_integer(ary->data[j]);
        }
        break;
      }
      case MRBC_TT_INTEGER: {
        buffer[pos++] = (uint8_t)mrbc_integer(arg);
        break;
      }
      case MRBC_TT_STRING: {
        const char *str = (const char *)arg.string->data;
        mrbc_int_t str_len = arg.string->size;
        memcpy(&buffer[pos], str, str_len);
        pos += str_len;
        break;
      }
      default:
        mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer, Array or String");
        return;
    }
  }

  // Fill remaining bytes with zeros (for additional read bytes)
  for (size_t i = 0; i < additional_zeros; i++) {
    buffer[pos++] = 0;
  }
}

static mrbc_int_t
mrbc_spi_write_outputs(mrbc_vm *vm, spi_unit_info_t *unit_info, mrbc_value *args, int argc)
{
  size_t total_size = mrbc_spi_calculate_buffer_size(vm, args, argc, 0);
  if (total_size == 0) return -1;

  // Choose buffer allocation strategy
  uint8_t stack_buffer[MAX_STACK_BUFFER_SIZE];
  uint8_t *buffer;
  bool needs_free = false;

  if (total_size < MAX_STACK_BUFFER_SIZE) {
    buffer = stack_buffer;
  } else {
    buffer = (uint8_t *)mrbc_alloc(vm, total_size);
    if (!buffer) {
      mrbc_raise(vm, MRBC_CLASS(StandardError), "Failed to allocate memory");
      return -1;
    }
    needs_free = true;
  }

  // Fill buffer with all arguments
  mrbc_spi_fill_buffer(vm, buffer, args, argc, 0);

  mrbc_int_t res = SPI_write_blocking(unit_info, buffer, total_size);

  if (needs_free) {
    mrbc_free(vm, buffer);
  }

  return res;
}

static void
c_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)v->instance->data;
  mrbc_int_t total_bytes = mrbc_spi_write_outputs(vm, unit_info, &v[1], argc);

  if (total_bytes < 0) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "SPI write failed");
    return;
  }

  SET_INT_RETURN(total_bytes);
}

static void
c_read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)v->instance->data;
  size_t len = GET_INT_ARG(1);
  uint8_t repeated_tx_data = 0;
  if (1 < argc) {
    repeated_tx_data = GET_INT_ARG(2);
  }
  uint8_t rxdata[len];
  int ret_len = SPI_read_blocking(unit_info, rxdata, len, repeated_tx_data);
  if (-1 < ret_len) {
    mrbc_value value = mrbc_string_new(vm, (const char *)rxdata, ret_len);
    SET_RETURN(value);
  } else {
    mrbc_raise(vm, MRBC_CLASS(IOError), "SPI read failed");
  }
}

static void
c_transfer(mrbc_vm *vm, mrbc_value *v, int argc)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)v->instance->data;
  int additional_read_bytes = 0;

  // Check for keyword arguments (additional_read_bytes)
  if (argc > 0) {
    mrbc_value *last_arg = &v[argc];
    if (last_arg->tt == MRBC_TT_HASH) {
      mrbc_value additional_read_bytes_key = mrbc_symbol_value(mrbc_str_to_symid("additional_read_bytes"));
      mrbc_value additional_read_bytes_val = mrbc_hash_get(last_arg, &additional_read_bytes_key);
      if (additional_read_bytes_val.tt == MRBC_TT_INTEGER) {
        additional_read_bytes = mrbc_integer(additional_read_bytes_val);
      }
      argc--; // Exclude hash from argument processing
    }
  }

  mrbc_value *args = &v[1];

  size_t total_size = mrbc_spi_calculate_buffer_size(vm, args, argc, additional_read_bytes);
  if (total_size == 0) return;

  // Choose buffer allocation strategy
  uint8_t stack_tx_buffer[MAX_STACK_BUFFER_SIZE];
  uint8_t stack_rx_buffer[MAX_STACK_BUFFER_SIZE];
  uint8_t *txdata, *rxdata;
  bool needs_free = false;

  if (total_size < MAX_STACK_BUFFER_SIZE) {
    txdata = stack_tx_buffer;
    rxdata = stack_rx_buffer;
  } else {
    txdata = (uint8_t *)mrbc_alloc(vm, total_size * 2);
    if (!txdata) {
      mrbc_raise(vm, MRBC_CLASS(StandardError), "Failed to allocate memory");
      return;
    }
    rxdata = txdata + total_size;
    needs_free = true;
  }

  // Fill buffer with all arguments plus additional zeros
  mrbc_spi_fill_buffer(vm, txdata, args, argc, additional_read_bytes);

  int ret_len = SPI_transfer(unit_info, txdata, rxdata, total_size);

  if (ret_len == total_size) {
    mrbc_value value = mrbc_string_new(vm, (const char *)rxdata, ret_len);
    SET_RETURN(value);
  } else {
    if (needs_free) mrbc_free(vm, txdata);
    mrbc_raise(vm, MRBC_CLASS(IOError), "SPI transfer failed");
    return;
  }

  if (needs_free) {
    mrbc_free(vm, txdata);
  }
}

static void
c_sck_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->sck_pin);
}

static void
c_copi_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->copi_pin);
}

static void
c_cipo_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->cipo_pin);
}

static void
c_cs_pin(mrbc_vm *vm, mrbc_value *v, int argc)
{
  spi_unit_info_t *unit_info = (spi_unit_info_t *)v->instance->data;
  SET_INT_RETURN(unit_info->cs_pin);
}

static void
c_s_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int unit_num = PICORUBY_SPI_BITBANG;
  if (strcmp((const char *)GET_STRING_ARG(1), "BITBANG") != 0) {
    unit_num = SPI_unit_name_to_unit_num((const char *)GET_STRING_ARG(1));
  }
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(spi_unit_info_t));
  spi_unit_info_t *unit_info = (spi_unit_info_t *)self.instance->data;
  memset(unit_info, 0, sizeof(spi_unit_info_t));
  unit_info->unit_num  = unit_num;
  unit_info->frequency = (uint32_t)GET_INT_ARG(2);
  unit_info->sck_pin   = (int8_t) GET_INT_ARG(3);
  unit_info->cipo_pin  = (int8_t) GET_INT_ARG(4);
  unit_info->copi_pin  = (int8_t) GET_INT_ARG(5);
  unit_info->cs_pin    = (int8_t) GET_INT_ARG(6);
  unit_info->mode      = (uint8_t)GET_INT_ARG(7);
  unit_info->first_bit = (uint8_t)GET_INT_ARG(8);
  unit_info->data_bits = (uint8_t)GET_INT_ARG(9);
  spi_status_t status = SPI_gpio_init(unit_info);
  if (status < 0) {
    const char *message;
    switch (status) {
      case SPI_ERROR_INVALID_UNIT: { message = "Invalid SPI unit"; break; }
      case SPI_ERROR_INVALID_MODE: { message = "Invalid SPI mode"; break; }
      case SPI_ERROR_INVALID_FIRST_BIT: { message = "Invalid SPI firt bit"; break; }
      default: { message = "Unknown SPI error"; }
    }
    mrbc_raise(vm, MRBC_CLASS(IOError), message);
    return;
  }
  SET_RETURN(self);
}

void
mrbc_spi_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_SPI = mrbc_define_class(vm, "SPI", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_SPI, "init", c_s_init);
  mrbc_define_method(vm, mrbc_class_SPI, "write", c_write);
  mrbc_define_method(vm, mrbc_class_SPI, "read", c_read);
  mrbc_define_method(vm, mrbc_class_SPI, "transfer", c_transfer);
  mrbc_define_method(vm, mrbc_class_SPI, "sck_pin", c_sck_pin);
  mrbc_define_method(vm, mrbc_class_SPI, "cipo_pin", c_cipo_pin);
  mrbc_define_method(vm, mrbc_class_SPI, "copi_pin", c_copi_pin);
  mrbc_define_method(vm, mrbc_class_SPI, "cs_pin", c_cs_pin);
}
