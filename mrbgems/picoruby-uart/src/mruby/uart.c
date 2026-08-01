#include <stddef.h>

#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/internal.h>
#include <mruby/variable.h>

static mrb_value
mrb_open_connection(mrb_state *mrb, mrb_value self)
{
  const char *unit_name;
  mrb_int txd_pin, rxd_pin;
  mrb_value buffer_size;
  size_t rx_buffer_size;
  mrb_get_args(mrb, "ziio", &unit_name, &txd_pin, &rxd_pin, &buffer_size);
  if (mrb_nil_p(buffer_size)) {
    rx_buffer_size = 0; // let the unit table pick the default
  } else if (mrb_fixnum_p(buffer_size)) {
    mrb_int requested = mrb_fixnum(buffer_size);
    if (requested <= 0) {
      struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
      mrb_raise(mrb, IOError, "UART: rx_buffer_size is not power of two");
    }
    rx_buffer_size = (size_t)requested;
  } else {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument type (expected Integer or nil)");
  }
  int unit_num = UART_resolve_unit_num(unit_name, (int)txd_pin, (int)rxd_pin);
  if (unit_num == UART_ERROR_UNIT_MISMATCH) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "UART: pins conflict with unit or are invalid for UART");
  } else if (unit_num == UART_ERROR_UNDETERMINED) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "UART: cannot determine unit; specify unit or valid pins");
  } else if (unit_num < 0) {
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, "UART: invalid unit name");
  }
  switch (UART_unit_open(unit_num, rx_buffer_size)) {
    case UART_ERROR_NONE:
      break;
    case UART_ERROR_BUFFER_INUSE:
      mrb_raise(mrb, E_ARGUMENT_ERROR,
                "UART: unit is already open with a different rx_buffer_size");
      break;
    case UART_ERROR_NOMEM:
      mrb_raise(mrb, E_RUNTIME_ERROR, "UART: failed to allocate rx buffer");
      break;
    case UART_ERROR_INVALID_UNIT: {
      struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
      mrb_raise(mrb, IOError, "UART: invalid unit name");
      break;
    }
    default: {
      struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
      mrb_raise(mrb, IOError, "UART: rx_buffer_size is not power of two");
      break;
    }
  }
  UART_open(unit_num, txd_pin, rxd_pin);
  return mrb_fixnum_value(unit_num);
}

static mrb_value
mrb__set_baudrate(mrb_state *mrb, mrb_value self)
{
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  mrb_int baudrate;
  mrb_get_args(mrb, "i", &baudrate);
  return mrb_fixnum_value(UART_set_baudrate(unit_num, (uint32_t)baudrate));
}

static mrb_value
mrb__set_flow_control(mrb_state *mrb, mrb_value self)
{
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  mrb_bool cts, rts;
  mrb_get_args(mrb, "bb", &cts, &rts);
  UART_set_flow_control(unit_num, cts, rts);
  return self;
}

static mrb_value
mrb__set_format(mrb_state *mrb, mrb_value self)
{
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  mrb_int data_bits, stop_bit, parity;
  mrb_get_args(mrb, "iii", &data_bits, &stop_bit, &parity);
  UART_set_format(unit_num, (int32_t)data_bits, (int32_t)stop_bit, (int32_t)parity);
  return self;
}

static mrb_value
mrb__set_function(mrb_state *mrb, mrb_value self)
{
  mrb_int pin;
  mrb_get_args(mrb, "i", &pin);
  UART_set_function(pin);
  return self;
}

static int
mrb_uart_unit_num(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
}

static mrb_value
mrb_read(mrb_state *mrb, mrb_value self)
{
  int unit_num = mrb_uart_unit_num(mrb, self);
  size_t available_len = UART_bytes_available(unit_num);
  if (available_len == 0) {
    return mrb_nil_value();
  }
  mrb_int len = 0;
  if (mrb_get_args(mrb, "|i", &len) == 1) {
    if (available_len < (size_t)len) {
      return mrb_nil_value();
    } else {
      available_len = len;
    }
  }
  uint8_t buf[available_len];
  available_len = UART_read(unit_num, buf, available_len);
  return mrb_str_new(mrb, (const char *)buf, available_len);
}

static mrb_value
mrb_readpartial(mrb_state *mrb, mrb_value self)
{
  int unit_num = mrb_uart_unit_num(mrb, self);
  mrb_int maxlen;
  mrb_get_args(mrb, "i", &maxlen);
  size_t available_len = UART_bytes_available(unit_num);
  if (available_len == 0) {
    return mrb_nil_value();
  }
  if (available_len < (size_t)maxlen) {
    maxlen = available_len;
  }
  uint8_t buf[maxlen];
  size_t len = UART_read(unit_num, buf, (size_t)maxlen);
  return mrb_str_new(mrb, (const char *)buf, len);
}

static mrb_value
mrb_getbyte(mrb_state *mrb, mrb_value self)
{
  uint8_t byte;
  if (!UART_getbyte(mrb_uart_unit_num(mrb, self), &byte)) {
    return mrb_nil_value();
  }
  return mrb_fixnum_value(byte);
}

static mrb_value
mrb_ungetbyte(mrb_state *mrb, mrb_value self)
{
  mrb_int value;
  mrb_get_args(mrb, "i", &value);
  if (!UART_ungetbyte(mrb_uart_unit_num(mrb, self), (uint8_t)(value & 0xff))) {
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, "UART: rx buffer is full");
  }
  return mrb_nil_value();
}

static mrb_value
mrb_bytes_available(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(UART_bytes_available(mrb_uart_unit_num(mrb, self)));
}

static mrb_value
mrb_last_read_timestamp_us(mrb_state *mrb, mrb_value self)
{
  uint64_t timestamp_us;
  if (!UART_lastReadTimestamp(mrb_uart_unit_num(mrb, self), &timestamp_us)) {
    return mrb_nil_value();
  }
  return mrb_int_value(mrb, (mrb_int)timestamp_us);
}

static mrb_value
mrb_rx_overflow_count(mrb_state *mrb, mrb_value self)
{
  return mrb_int_value(mrb, (mrb_int)UART_rxOverflowCount(mrb_uart_unit_num(mrb, self)));
}

static mrb_value
mrb_write(mrb_state *mrb, mrb_value self)
{
  mrb_value str;
  mrb_get_args(mrb, "S", &str);
  size_t len = RSTRING_LEN(str);
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  UART_write_blocking(unit_num, (const uint8_t *)RSTRING_PTR(str), len);
  return mrb_fixnum_value(len);
}

static mrb_value
mrb_putc(mrb_state *mrb, mrb_value self)
{
  mrb_value ch;
  mrb_get_args(mrb, "o", &ch);

  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  if (mrb_integer_p(ch)) {
    uint8_t byte = (uint8_t)(mrb_integer(ch) & 0xff);
    UART_write_blocking(unit_num, &byte, 1);
    return ch;
  }
  if (!mrb_string_p(ch)) {
    mrb_raise(mrb, E_TYPE_ERROR, "wrong argument type (expected Integer or String)");
  }

  const char *ptr = RSTRING_PTR(ch);
  mrb_int len = RSTRING_LEN(ch);
  if (len == 0) {
    return ch;
  }
#ifdef MRB_UTF8_STRING
  len = mrb_utf8len(ptr, ptr + len);
#else
  len = 1;
#endif
  UART_write_blocking(unit_num, (const uint8_t *)ptr, len);
  return ch;
}

static mrb_value
mrb_gets(mrb_state *mrb, mrb_value self)
{
  int unit_num = mrb_uart_unit_num(mrb, self);
  int pos = UART_line_length(unit_num);
  if (pos < 0) {
    return mrb_nil_value();
  }
  uint8_t buf[pos];
  size_t len = UART_read(unit_num, buf, (size_t)pos);
  return mrb_str_new(mrb, (const char *)buf, len);
}

static mrb_value
mrb_flush(mrb_state *mrb, mrb_value self)
{
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  UART_flush(unit_num);
  return self;
}

static mrb_value
mrb_clear_tx_buffer(mrb_state *mrb, mrb_value self)
{
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  UART_clear_tx_buffer(unit_num);
  return self;
}

static mrb_value
mrb_clear_rx_buffer(mrb_state *mrb, mrb_value self)
{
  UART_clear_rx(mrb_uart_unit_num(mrb, self));
  return self;
}

static mrb_value
mrb_break(mrb_state *mrb, mrb_value self)
{
  uint32_t break_ms;
  mrb_value break_ms_val;
  if (mrb_get_args(mrb, "|o", &break_ms_val) == 0) {
    break_ms = 100;
  } else {
    if (mrb_float_p(break_ms_val)) {
      break_ms = (uint32_t)(mrb_float(break_ms_val) * 1000);
    } else if (mrb_fixnum_p(break_ms_val)) {
      break_ms = (uint32_t)(mrb_fixnum(break_ms_val) * 1000);
    } else {
      mrb_raise(mrb, E_TYPE_ERROR, "can't convert into time interval");
    }
  }
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  UART_break(unit_num, break_ms);
  return self;
}


void
mrb_picoruby_uart_gem_init(mrb_state* mrb)
{
  /* No MRB_SET_INSTANCE_TT: the RX ring belongs to the unit now, so a
     UART carries no C data and has no free function that could pull the
     ring out from under the ISR. */
  struct RClass *class_UART = mrb_define_class_id(mrb, MRB_SYM(UART), mrb->object_class);

  mrb_define_const_id(mrb, class_UART, MRB_SYM(PARITY_NONE), mrb_fixnum_value(PARITY_NONE));
  mrb_define_const_id(mrb, class_UART, MRB_SYM(PARITY_EVEN), mrb_fixnum_value(PARITY_EVEN));
  mrb_define_const_id(mrb, class_UART, MRB_SYM(PARITY_ODD), mrb_fixnum_value(PARITY_ODD));
  mrb_define_const_id(mrb, class_UART, MRB_SYM(FLOW_CONTROL_NONE), mrb_fixnum_value(FLOW_CONTROL_NONE));
  mrb_define_const_id(mrb, class_UART, MRB_SYM(FLOW_CONTROL_RTS_CTS), mrb_fixnum_value(FLOW_CONTROL_RTS_CTS));

  mrb_define_private_method_id(mrb, class_UART, MRB_SYM(open_connection), mrb_open_connection, MRB_ARGS_REQ(4));
  mrb_define_private_method_id(mrb, class_UART, MRB_SYM(_set_baudrate), mrb__set_baudrate, MRB_ARGS_REQ(1));
  mrb_define_private_method_id(mrb, class_UART, MRB_SYM(_set_flow_control), mrb__set_flow_control, MRB_ARGS_REQ(2));
  mrb_define_private_method_id(mrb, class_UART, MRB_SYM(_set_format), mrb__set_format, MRB_ARGS_REQ(3));
  mrb_define_private_method_id(mrb, class_UART, MRB_SYM(_set_function), mrb__set_function, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(read), mrb_read, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(readpartial), mrb_readpartial, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(getbyte), mrb_getbyte, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(ungetbyte), mrb_ungetbyte, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(bytes_available), mrb_bytes_available, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(last_read_timestamp_us), mrb_last_read_timestamp_us, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(rx_overflow_count), mrb_rx_overflow_count, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(write), mrb_write, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(putc), mrb_putc, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(gets), mrb_gets, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(flush), mrb_flush, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(clear_tx_buffer), mrb_clear_tx_buffer, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(clear_rx_buffer), mrb_clear_rx_buffer, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(break), mrb_break, MRB_ARGS_OPT(1));
}

void
mrb_picoruby_uart_gem_final(mrb_state* mrb)
{
}
