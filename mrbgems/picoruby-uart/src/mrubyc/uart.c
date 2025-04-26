#include <mrubyc.h>

static mrbc_class *mrbc_class_UART;
static mrbc_class *mrbc_class_UART_RxBuffer;

#define GETIV(str)  mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define SETIV(str, val) mrbc_instance_setiv(&v[0], mrbc_str_to_symid(#str), val)

static void
c_open_rx_buffer(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int rx_buffer_size;
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments. expected 1");
    return;
  } else
  if (v[1].tt == MRBC_TT_NIL) {
    rx_buffer_size = PICORUBY_UART_RX_BUFFER_SIZE;
  } else {
    rx_buffer_size = GET_INT_ARG(1);
  }
  mrbc_value rx_buffer_value = mrbc_instance_new(vm, mrbc_class_UART_RxBuffer, sizeof(RingBuffer) + sizeof(uint8_t) * rx_buffer_size);

  RingBuffer *rx = (RingBuffer *)rx_buffer_value.instance->data;
  if (!initializeBuffer(rx, rx_buffer_size)) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "UART: rx_buffer_size is not power of two");
    return;
  }
  SET_RETURN(rx_buffer_value);
}

static void
c_open_connection(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int unit_num = UART_unit_name_to_unit_num((const char *)GET_STRING_ARG(1));
  RingBuffer *rx = (RingBuffer *)v[4].instance->data;
  UART_init(unit_num, GET_INT_ARG(2), GET_INT_ARG(3), rx);
  SET_INT_RETURN(unit_num);
}

static void
c__set_baudrate(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int unit_num = GETIV(unit_num).i;
  uint32_t baudrate = GET_INT_ARG(1);
  SET_INT_RETURN(UART_set_baudrate(unit_num, baudrate));
}

static void
c__set_flow_control(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int unit_num = GETIV(unit_num).i;
  bool cts = (v[1].tt == MRBC_TT_TRUE);
  bool rts = (v[2].tt == MRBC_TT_TRUE);
  UART_set_flow_control(unit_num, cts, rts);
}

static void
c__set_format(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int unit_num = GETIV(unit_num).i;
  int32_t data_bits = GET_INT_ARG(1);
  int32_t stop_bits = GET_INT_ARG(2);
  int32_t parity = GET_INT_ARG(3);
  UART_set_format(unit_num, data_bits, stop_bits, parity);
}

static void
c__set_function(mrbc_vm *vm, mrbc_value v[], int argc)
{
  UART_set_function(GET_INT_ARG(1));
}

static void
c_read(mrbc_vm *vm, mrbc_value v[], int argc)
{
  RingBuffer *rx = (RingBuffer *)GETIV(rx_buffer).instance->data;
  size_t available_len = bufferDataSize(rx);
  if (available_len == 0) {
    SET_NIL_RETURN();
    return;
  }
  if (argc == 1) {
    size_t len = GET_INT_ARG(1);
    if (available_len < len) {
      SET_NIL_RETURN();
      return;
    } else {
      available_len = len;
    }
  }
  uint8_t buf[available_len];
  popBuffer(rx, buf, available_len);
  mrbc_value str = mrbc_string_new(vm, buf, available_len);
  SET_RETURN(str);
}

static void
c_readpartial(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments. expected 1");
    return;
  }
  RingBuffer *rx = (RingBuffer *)GETIV(rx_buffer).instance->data;
  size_t available_len = bufferDataSize(rx);
  if (available_len == 0) {
    SET_NIL_RETURN();
    return;
  }
  size_t maxlen = GET_INT_ARG(1);
  if (available_len < maxlen) {
    maxlen = available_len;
  }
  uint8_t buf[maxlen];
  popBuffer(rx, buf, maxlen);
  mrbc_value str = mrbc_string_new(vm, buf, maxlen);
  SET_RETURN(str);
}

static void
c_bytes_available(mrbc_vm *vm, mrbc_value v[], int argc)
{
  RingBuffer *rx = (RingBuffer *)GETIV(rx_buffer).instance->data;
  SET_INT_RETURN(bufferDataSize(rx));
}

static void
c_write(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments. expected 1");
    return;
  }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type (expected String)");
    return;
  }
  size_t len = v[1].string->size;
  int unit_num = GETIV(unit_num).i;
  UART_write_blocking(unit_num, (const uint8_t *)v[1].string->data, len);
  SET_INT_RETURN(len);
}

static void
c_gets(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (0 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments. expected 0");
    return;
  }
  RingBuffer *rx = (RingBuffer *)GETIV(rx_buffer).instance->data;
  int pos = searchCharBuffer(rx, (uint8_t)'\n');
  if (pos < 0) {
    SET_NIL_RETURN();
    return;
  }
  pos++;
  uint8_t buf[pos];
  popBuffer(rx, buf, pos);
  mrbc_value str = mrbc_string_new(vm, buf, pos);
  SET_RETURN(str);
}

static void
c_flush(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int unit_num = GETIV(unit_num).i;
  UART_flush(unit_num);
}

static void
c_clear_tx_buffer(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int unit_num = GETIV(unit_num).i;
  UART_clear_tx_buffer(unit_num);
}

static void
c_clear_rx_buffer(mrbc_vm *vm, mrbc_value v[], int argc)
{
  RingBuffer *rx = (RingBuffer *)GETIV(rx_buffer).instance->data;
  clearBuffer(rx);
  int unit_num = GETIV(unit_num).i;
  UART_clear_rx_buffer(unit_num);
}

static void
c_break(mrbc_vm *vm, mrbc_value v[], int argc)
{
  uint32_t break_ms;
  if (argc == 0) {
    break_ms = 100;
  } else if (1 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments. expected 0..1");
    return;
  } else {
    if (v[1].tt == MRBC_TT_FLOAT) {
      break_ms = (uint32_t)(GET_FLOAT_ARG(1) * 1000);
    } else if (v[1].tt == MRBC_TT_INTEGER) {
      break_ms = GET_INT_ARG(1) * 1000;
    } else {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "can't convert into time interval");
      return;
    }
  }
  int unit_num = GETIV(unit_num).i;
  UART_break(unit_num, break_ms);
}

#define SET_CLASS_CONST_INT(cls, cst) \
  mrbc_set_class_const(mrbc_class_##cls, mrbc_str_to_symid(#cst), &mrbc_integer_value(cst))

void
mrbc_uart_init(mrbc_vm *vm)
{
  mrbc_class_UART = mrbc_define_class(vm, "UART", mrbc_class_object);
  mrbc_class_UART_RxBuffer = mrbc_define_class(vm, "RxBuffer", mrbc_class_UART);

  SET_CLASS_CONST_INT(UART, PARITY_NONE);
  SET_CLASS_CONST_INT(UART, PARITY_EVEN);
  SET_CLASS_CONST_INT(UART, PARITY_ODD);
  SET_CLASS_CONST_INT(UART, FLOW_CONTROL_NONE);
  SET_CLASS_CONST_INT(UART, FLOW_CONTROL_RTS_CTS);

  mrbc_define_method(vm, mrbc_class_UART, "open_rx_buffer", c_open_rx_buffer);
  mrbc_define_method(vm, mrbc_class_UART, "open_connection", c_open_connection);
  mrbc_define_method(vm, mrbc_class_UART, "_set_baudrate", c__set_baudrate);
  mrbc_define_method(vm, mrbc_class_UART, "_set_flow_control", c__set_flow_control);
  mrbc_define_method(vm, mrbc_class_UART, "_set_format", c__set_format);
  mrbc_define_method(vm, mrbc_class_UART, "_set_function", c__set_function);
  mrbc_define_method(vm, mrbc_class_UART, "read", c_read);
  mrbc_define_method(vm, mrbc_class_UART, "readpartial", c_readpartial);
  mrbc_define_method(vm, mrbc_class_UART, "bytes_available", c_bytes_available);
  mrbc_define_method(vm, mrbc_class_UART, "write", c_write);
  mrbc_define_method(vm, mrbc_class_UART, "gets", c_gets);
  mrbc_define_method(vm, mrbc_class_UART, "flush", c_flush);
  mrbc_define_method(vm, mrbc_class_UART, "clear_tx_buffer", c_clear_tx_buffer);
  mrbc_define_method(vm, mrbc_class_UART, "clear_rx_buffer", c_clear_rx_buffer);
  mrbc_define_method(vm, mrbc_class_UART, "break", c_break);
}
