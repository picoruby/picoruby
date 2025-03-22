#include <stddef.h>

#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/variable.h>

static void
mrb_uart_rx_buffer_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_uart_rx_buffer_type = {
  "UART", mrb_uart_rx_buffer_free,
};

static mrb_value
mrb_open_rx_buffer(mrb_state *mrb, mrb_value self)
{
  int rx_buffer_size;
  mrb_value buffer_size;
  mrb_get_args(mrb, "|o", &buffer_size);
  if (mrb_nil_p(buffer_size)) {
    rx_buffer_size = PICORUBY_UART_RX_BUFFER_SIZE;
  } else {
    rx_buffer_size = mrb_fixnum(buffer_size);
  }
  RingBuffer *rx = (RingBuffer *)mrb_malloc(mrb, sizeof(RingBuffer) + sizeof(uint8_t) * rx_buffer_size);
  if (!initializeBuffer(rx, rx_buffer_size)) {
    struct RClass *IOError = mrb_exc_get_id(mrb, MRB_SYM(IOError));
    mrb_raise(mrb, IOError, "UART: rx_buffer_size is not power of two");
  }
  DATA_PTR(self) = rx;
  DATA_TYPE(self) = &mrb_uart_rx_buffer_type;
  return mrb_true_value(); // for compatibility with mruby/c
}

static mrb_value
mrb_open_connection(mrb_state *mrb, mrb_value self)
{
  const char *unit_name;
  mrb_int txd_pin, rxd_pin;
  mrb_get_args(mrb, "zii", &unit_name, &txd_pin, &rxd_pin);
  int unit_num = UART_unit_name_to_unit_num(unit_name);
  RingBuffer *rx = (RingBuffer *)mrb_data_get_ptr(mrb, self, &mrb_uart_rx_buffer_type);
  UART_init(unit_num, txd_pin, rxd_pin, rx);
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

static mrb_value
mrb_read(mrb_state *mrb, mrb_value self)
{
  RingBuffer *rx = (RingBuffer *)mrb_data_get_ptr(mrb, self, &mrb_uart_rx_buffer_type);
  size_t available_len = bufferDataSize(rx);
  if (available_len == 0) {
    return mrb_nil_value();
  }
  mrb_value len_val;
  mrb_int len;
  mrb_get_args(mrb, "|o", &len_val);
  if (mrb_fixnum_p(len_val)) {
    len = mrb_fixnum(len_val);
    if (available_len < len) {
      return mrb_nil_value();
    } else {
      available_len = len;
    }
  }
  uint8_t buf[available_len];
  popBuffer(rx, buf, available_len);
  return mrb_str_new(mrb, (const char *)buf, available_len);
}

static mrb_value
mrb_readpartial(mrb_state *mrb, mrb_value self)
{
  RingBuffer *rx = (RingBuffer *)mrb_data_get_ptr(mrb, self, &mrb_uart_rx_buffer_type);
  mrb_int maxlen;
  mrb_get_args(mrb, "i", &maxlen);
  size_t available_len = bufferDataSize(rx);
  if (available_len == 0) {
    return mrb_nil_value();
  }
  if (available_len < maxlen) {
    maxlen = available_len;
  }
  uint8_t buf[maxlen];
  popBuffer(rx, buf, maxlen);
  return mrb_str_new(mrb, (const char *)buf, maxlen);
}

static mrb_value
mrb_bytes_available(mrb_state *mrb, mrb_value self)
{
  RingBuffer *rx = (RingBuffer *)mrb_data_get_ptr(mrb, self, &mrb_uart_rx_buffer_type);
  return mrb_fixnum_value(bufferDataSize(rx));
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
mrb_gets(mrb_state *mrb, mrb_value self)
{
  RingBuffer *rx = (RingBuffer *)mrb_data_get_ptr(mrb, self, &mrb_uart_rx_buffer_type);
  int pos = searchCharBuffer(rx, (uint8_t)'\n');
  if (pos < 0) {
    return mrb_nil_value();
  }
  pos++;
  uint8_t buf[pos];
  popBuffer(rx, buf, pos);
  return mrb_str_new(mrb, (const char *)buf, pos);
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
  RingBuffer *rx = (RingBuffer *)mrb_data_get_ptr(mrb, self, &mrb_uart_rx_buffer_type);
  clearBuffer(rx);
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  UART_clear_rx_buffer(unit_num);
  return self;
}

static mrb_value
mrb_break(mrb_state *mrb, mrb_value self)
{
  uint32_t break_ms;
  mrb_value break_ms_val;
  mrb_get_args(mrb, "|o", &break_ms_val);
  if (mrb_nil_p(break_ms_val)) {
    break_ms = 100;
  } else {
    if (mrb_float_p(break_ms_val)) {
      break_ms = (uint32_t)(mrb_float(break_ms_val) * 1000);
    } else if (mrb_fixnum_p(break_ms_val)) {
      break_ms = (uint32_t)mrb_fixnum(break_ms_val);
    } else {
      mrb_raise(mrb, E_TYPE_ERROR, "can't convert into time interval");
    }
  }
  int unit_num = mrb_fixnum(mrb_iv_get(mrb, self, MRB_IVSYM(unit_num)));
  UART_break(unit_num, (uint32_t)break_ms);
  return self;
}


void
mrb_picoruby_uart_gem_init(mrb_state* mrb)
{
  struct RClass *class_UART = mrb_define_class_id(mrb, MRB_SYM(UART), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_UART, MRB_TT_CDATA);

  mrb_define_const_id(mrb, class_UART, MRB_SYM(PARITY_NONE), mrb_fixnum_value(PARITY_NONE));
  mrb_define_const_id(mrb, class_UART, MRB_SYM(PARITY_EVEN), mrb_fixnum_value(PARITY_EVEN));
  mrb_define_const_id(mrb, class_UART, MRB_SYM(PARITY_ODD), mrb_fixnum_value(PARITY_ODD));
  mrb_define_const_id(mrb, class_UART, MRB_SYM(FLOW_CONTROL_NONE), mrb_fixnum_value(FLOW_CONTROL_NONE));
  mrb_define_const_id(mrb, class_UART, MRB_SYM(FLOW_CONTROL_RTS_CTS), mrb_fixnum_value(FLOW_CONTROL_RTS_CTS));

  mrb_define_method_id(mrb, class_UART, MRB_SYM(open_rx_buffer), mrb_open_rx_buffer, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(open_connection), mrb_open_connection, MRB_ARGS_REQ(3));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(_set_baudrate), mrb__set_baudrate, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(_set_flow_control), mrb__set_flow_control, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(_set_format), mrb__set_format, MRB_ARGS_REQ(3));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(_set_function), mrb__set_function, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(read), mrb_read, MRB_ARGS_OPT(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(readpartial), mrb_readpartial, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_UART, MRB_SYM(bytes_available), mrb_bytes_available, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_UART, MRB_SYM(write), mrb_write, MRB_ARGS_REQ(1));
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
