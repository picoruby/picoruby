#include <mrubyc.h>
#include <alloc.h>

#include "../include/rng.h"

static void
c_rng_random_int(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t bytes[4];
  uint32_t ret = 0;

  /* The failure-reporting call, not the byte one: a source that never
     became ready would otherwise hand back zeros and this would return
     0 as if it were a draw. */
  if (rng_random_bytes_impl(bytes, sizeof(bytes)) != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "entropy source failed");
    return;
  }
  for (int i = 0; i < 4; i++) {
    ret = (ret << 8) | bytes[i];
  }
  SET_INT_RETURN(ret);
}

static void
c_rng_random_string(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value len = GET_ARG(1);
  if (len.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  unsigned char* buf = mrbc_alloc(vm, len.i);
  if (rng_random_bytes_impl((uint8_t *)buf, (size_t)len.i) != 0) {
    mrbc_free(vm, buf);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "entropy source failed");
    return;
  }
  mrbc_value ret = mrbc_string_new(vm, buf, len.i);
  mrbc_free(vm, buf);
  SET_RETURN(ret);
}

void
mrbc_rng_init(mrbc_vm *vm)
{
  mrbc_class *module_RNG = mrbc_define_module(vm, "RNG");

  mrbc_define_method(vm, module_RNG, "random_int", c_rng_random_int);
  mrbc_define_method(vm, module_RNG, "random_string", c_rng_random_string);
}
