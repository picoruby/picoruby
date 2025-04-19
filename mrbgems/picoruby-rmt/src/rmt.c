#include <mrubyc.h>

#include "rmt.h"

static void
c_rmt__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  RMT_symbol_dulation_t rmt_symbol_dulation = {
    .t0h_ns = GET_INT_ARG(2),
    .t0l_ns = GET_INT_ARG(3),
    .t1h_ns = GET_INT_ARG(4),
    .t1l_ns = GET_INT_ARG(5),
    .reset_ns = GET_INT_ARG(6)
  };

  int ret = RMT_init((uint32_t)GET_INT_ARG(1), &rmt_symbol_dulation);
  SET_INT_RETURN(ret);
}

static void
c_rmt__write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_array value_ary = *(GET_ARY_ARG(1).array);
  int len = value_ary.n_stored;
  uint8_t txdata[len];

  for (int i = 0 ; i < len ; i++) {
    txdata[i] = mrbc_integer(value_ary.data[i]);
  }

  int ret = RMT_write(txdata, len);
  SET_INT_RETURN(ret);
}

void
mrbc_rmt_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_RMT = mrbc_define_class(vm, "RMT", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_RMT, "_init", c_rmt__init);
  mrbc_define_method(vm, mrbc_class_RMT, "_write", c_rmt__write);
}
