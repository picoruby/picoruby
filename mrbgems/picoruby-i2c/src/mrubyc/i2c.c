#include <mrubyc.h>

#define MAX_STACK_BUFFER_SIZE 256

static mrbc_int_t
mrbc_i2c_write_outputs(mrbc_vm *vm, mrbc_int_t unit_num, mrbc_int_t i2c_adrs_7, mrbc_value *args, int argc, mrbc_int_t timeout_ms, bool nostop)
{

  // Calculate total buffer size needed
  size_t total_size = 0;
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
        return -1;
    }
  }

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
  size_t pos = 0;

  for (int i = 0; i < argc; i++) {
    mrbc_value arg = args[i];
    switch (arg.tt) {
      case MRBC_TT_ARRAY: {
        mrbc_array *ary = arg.array;
        mrbc_int_t ary_len = ary->n_stored;
        for (mrbc_int_t j = 0; j < ary_len; j++) {
          if (ary->data[j].tt != MRBC_TT_INTEGER) {
            if (needs_free) mrbc_free(vm, buffer);
            mrbc_raise(vm, MRBC_CLASS(TypeError), "array element must be Integer");
            return -1;
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
        // This case should never be reached due to earlier checks
        if (needs_free) mrbc_free(vm, buffer);
        mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be Integer, Array or String");
        return -1;
    }
  }

  // Send as single I2C transaction
  mrbc_int_t res = I2C_write_timeout_us(
      (uint8_t)unit_num,
      (uint8_t)i2c_adrs_7,
      buffer,
      total_size,
      nostop,
      (uint32_t)timeout_ms * 1000
    );
  if (needs_free) {
    mrbc_free(vm, buffer);
  }
  
  if (res < 0) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "I2C write failed");
    return -1;
  }
  
  return res;
}

static void
c_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_int_t i2c_adrs_7 = GET_INT_ARG(1);
  mrbc_int_t timeout_ms = mrbc_integer(mrbc_instance_getiv(&v[0], mrbc_str_to_symid("timeout")));

  // Check for keyword arguments (timeout only)
  if (argc > 1) {
    mrbc_value *last_arg = &v[argc];
    if (last_arg->tt == MRBC_TT_HASH) {
      mrbc_value timeout_key = mrbc_symbol_value(mrbc_str_to_symid("timeout"));
      mrbc_value timeout_val = mrbc_hash_get(last_arg, &timeout_key);
      if (timeout_val.tt == MRBC_TT_INTEGER) {
        timeout_ms = mrbc_integer(timeout_val);
      }
      argc--; // Exclude hash from argument processing
    }
  }

  mrbc_int_t unit_num = mrbc_integer(mrbc_instance_getiv(&v[0], mrbc_str_to_symid("unit_num")));
  mrbc_int_t total_bytes = mrbc_i2c_write_outputs(vm, unit_num, i2c_adrs_7, &v[2], argc - 1, timeout_ms, false);
  if (total_bytes < 0) {
    return; // Error already raised in mrbc_i2c_write_outputs
  }
  SET_INT_RETURN(total_bytes);
}

static void
c_read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_int_t i2c_adrs_7 = GET_INT_ARG(1);
  mrbc_int_t len = GET_INT_ARG(2);
  mrbc_int_t unit_num = mrbc_integer(mrbc_instance_getiv(&v[0], mrbc_str_to_symid("unit_num")));
  mrbc_int_t timeout_ms = mrbc_integer(mrbc_instance_getiv(&v[0], mrbc_str_to_symid("timeout")));

  // Check for keyword arguments (timeout)
  if (argc > 2) {
    mrbc_value *last_arg = &v[argc];
    if (last_arg->tt == MRBC_TT_HASH) {
      mrbc_value timeout_key = mrbc_symbol_value(mrbc_str_to_symid("timeout"));

      mrbc_value timeout_val = mrbc_hash_get(last_arg, &timeout_key);
      if (timeout_val.tt == MRBC_TT_INTEGER) {
        timeout_ms = mrbc_integer(timeout_val);
      }

      argc--; // Exclude hash from argument processing
    }
  }

  // Send outputs if provided using mrbc_i2c_write_outputs with nostop: true
  if (argc > 2) {
    mrbc_int_t write_result = mrbc_i2c_write_outputs(vm, unit_num, i2c_adrs_7, &v[3], argc - 2, timeout_ms, true);
    if (write_result < 0) {
      return; // Error already raised in mrbc_i2c_write_outputs
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
    mrbc_value value = mrbc_string_new(vm, (const char *)rxdata, ret);
    SET_RETURN(value);
  } else {
    mrbc_raise(vm, MRBC_CLASS(IOError), "I2C read failed");
  }
}

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int unit_num = I2C_unit_name_to_unit_num((const char *)GET_STRING_ARG(1));
  uint32_t frequency = (uint32_t)GET_INT_ARG(2);
  i2c_status_t status = I2C_gpio_init(unit_num, frequency, (uint8_t)GET_INT_ARG(3), (uint8_t)GET_INT_ARG(4));
  if (status < 0) {
    char message[30];
    switch (status) {
      case I2C_ERROR_INVALID_UNIT: { strcpy(message, "Invalid I2C unit"); break; }
      default: { strcpy(message, "Unknows I2C error"); }
      mrbc_raise(vm, MRBC_CLASS(IOError), message);
      return;
    }
  }
  SET_INT_RETURN(unit_num);
}

void
mrbc_i2c_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_I2C = mrbc_define_class(vm, "I2C", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_I2C, "_init", c__init);
  mrbc_define_method(vm, mrbc_class_I2C, "write", c_write);
  mrbc_define_method(vm, mrbc_class_I2C, "read", c_read);
}


