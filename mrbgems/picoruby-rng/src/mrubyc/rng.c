#include <mrubyc.h>
#include <alloc.h>

#include "../include/rng.h"

static void
c_rng_random_int(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint32_t ret = 0;
  for (int i = 0; i < 4; i++) {
    ret = (ret << 8) | rng_random_byte_impl();
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
  for (int i = 0; i < len.i; i++) {
    buf[i] = (unsigned char)rng_random_byte_impl();
  }
  mrbc_value ret = mrbc_string_new(vm, buf, len.i);
  mrbc_free(vm, buf);
  SET_RETURN(ret);
}

void
mrbc_rng_init(mrbc_vm *vm)
{
  mrbc_class *class_RNG = mrbc_define_class(vm, "RNG", mrbc_class_object);

  mrbc_define_method(vm, class_RNG, "random_int", c_rng_random_int);
  mrbc_define_method(vm, class_RNG, "random_string", c_rng_random_string);
}
