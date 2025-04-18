#include "mrubyc.h"

/*
 * CRC.crc32(string = nil, crc = nil) -> Integer
 * when string is nil, returns the initial checksum value
 */
static void
c_crc_crc32(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value string = GET_ARG(1);
  mrbc_int_t crc = GET_INT_ARG(2);
  if (string.tt == MRBC_TT_NIL) {
    return;
  } else if (string.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "string expected");
    return;
  }
  uint32_t crc_value = generate_crc32((uint8_t *)string.string->data, string.string->size, crc);
  SET_INT_RETURN(crc_value);
}

void
mrbc_crc_init(mrbc_vm *vm)
{
  mrbc_class *module_CRC = mrbc_define_module(vm, "CRC");

  mrbc_define_method(vm, module_CRC, "crc32", c_crc_crc32);
}
