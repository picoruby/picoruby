#include <stdbool.h>
#include <mrubyc.h>
#include "../include/uart.h"

#define BUFFER_SIZE 256

typedef struct {
  uint8_t buffer[BUFFER_SIZE];
  int head;
  int tail;
  int size;
} RingBuffer;

static void
addToBuffer(RingBuffer *rb, uint8_t data)
{
  rb->buffer[rb->tail] = data;
  rb->tail = (rb->tail + 1) % rb->size;
  if (rb->tail == rb->head) {
    rb->head = (rb->head + 1) % rb->size;
  }
}

static uint8_t
removeFromBuffer(RingBuffer *rb)
{
  if (rb->head == rb->tail) {
    return '\0'';
  }
  uint8_t data = rb->buffer[rb->head];
  rb->head = (rb->head + 1) % rb->size;
  return data;
}

static int
getBufferSize(RingBuffer *rb)
{
  return (rb->tail - rb->head + rb->size) % rb->size;
}

static void
c__init(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value uart = mrbc_instance_new(vm, v->cls, sizeof(RingBuffer));
  RingBuffer *rb = (RingBuffer *)uart.instance->data;
  rb->head = 0;
  rb->tail = 0;
  rb->size = BUFFER_SIZE;
  int unit_num = UART_unit_name_to_unit_num((const char *)GET_STRING_ARG(1));
  int32_t parity = GET_INT_ARG(2);
  int32_t flow_control = GET_INT_ARG(3);

  uart_init(baudrate, parity, flow_control);
  SET_INT_RETURN(unit_num);
}

static void
c__set_baudrate(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int unit_num = GET_INT_ARG(1);
  int32_t baudrate = GET_INT_ARG(2);

  uart_set_baudrate(unit_num, baudrate);
}

static void
c__set_flow_control(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int unit_num = GET_INT_ARG(1);
  int32_t flow_control = GET_INT_ARG();

  uart_set_flow_control(flow_control);
}

static void
c__set_function(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int32_t function = GET_INT_ARG(1);

  uart_set_function(function);
}

static void
c__set_format(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int32_t data_bits = GET_INT_ARG(1);
  int32_t stop_bits = GET_INT_ARG(2);
  int32_t parity = GET_INT_ARG(3);

  uart_set_format(data_bits, stop_bits, parity);
}

static void
c_read(mrbc_vm *vm, mrbc_value v[], int argc)
{
  RingBuffer *rb = (RingBuffer *)v->instance->data;
  int32_t length = GET_INT_ARG(1);
  char *buf = (char *)mrbc_alloc(vm, length);

  uart_read(buf, length);
  SET_RETURN_STRING(buf);
}

static void
c_readpartial(mrbc_vm *vm, mrbc_value v[], int argc)
{
  RingBuffer *rb = (RingBuffer *)v->instance->data;
  int32_t length = GET_INT_ARG(1);
  char *buf = (char *)mrbc_alloc(vm, length);

  uart_readpartial(buf, length);
  SET_RETURN_STRING(buf);
}

static void
c_bytes_available(mrbc_vm *vm, mrbc_value v[], int argc)
{
  RingBuffer *rb = (RingBuffer *)v->instance->data;
  int32_t bytes_available = uart_bytes_available();

  SET_RETURN_INT(bytes_available);
}

static void
c_write(mrbc_vm *vm, mrbc_value v[], int argc)
{
  char *buf = GET_STRING_ARG(1);

  uart_write(buf);
}

static void
c_flush(mrbc_vm *vm, mrbc_value v[], int argc)
{
  uart_flush();
}

static void
c_clear_tx_buffer(mrbc_vm *vm, mrbc_value v[], int argc)
{
  uart_clear_tx_buffer();
}

static void
c_clear_rx_buffer(mrbc_vm *vm, mrbc_value v[], int argc)
{
  RingBuffer *rb = (RingBuffer *)v->instance->data;
  uart_clear_rx_buffer();
}

static void
c_break(mrbc_vm *vm, mrbc_value v[], int argc)
{
  UART_set_break();
}

#define SET_CLASS_CONST_INT(cls, cst) \
  mrbc_set_class_const(mrbc_class_##cls, mrbc_str_to_symid(#cst), &mrbc_integer_value(cst))

void
mrbc_uart_init(void)
{
  mrbc_class *mrbc_class_UART = mrbc_define_class(0, "UART", mrbc_class_object);

  SET_CLASS_CONST_INT(UART, PARITY_NONE);
  SET_CLASS_CONST_INT(UART, PARITY_EVEN);
  SET_CLASS_CONST_INT(UART, PARITY_ODD);
  SET_CLASS_CONST_INT(UART, FLOW_CONTROL_NONE);
  SET_CLASS_CONST_INT(UART, FLOW_CONTROL_RTS_CTS);

  mrbc_define_method(0, mrbc_class_UART, "_init", c__init);
  mrbc_define_method(0, mrbc_class_UART, "_set_baudrate", c__set_baudrate);
  mrbc_define_method(0, mrbc_class_UART, "_set_flow_control", c__set_flow_control);
  mrbc_define_method(0, mrbc_class_UART, "_set_function", c__set_function);
  mrbc_define_method(0, mrbc_class_UART, "_set_format", c__set_format);
  mrbc_define_method(0, mrbc_class_UART, "read", c_read);
  mrbc_define_method(0, mrbc_class_UART, "readpartial", c_readpartial);
  mrbc_define_method(0, mrbc_class_UART, "bytes_available", c_bytes_available);
  mrbc_define_method(0, mrbc_class_UART, "write", c_write);
  mrbc_define_method(0, mrbc_class_UART, "flush", c_flush);
  mrbc_define_method(0, mrbc_class_UART, "clear_tx_buffer", c_clear_tx_buffer);
  mrbc_define_method(0, mrbc_class_UART, "clear_rx_buffer", c_clear_rx_buffer);
  mrbc_define_method(0, mrbc_class_UART, "break", c_break);
}

