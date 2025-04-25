#include "mrubyc.h"

/*
 * CRC.crc32(string = nil, crc = nil) -> Integer
 * when string is nil, returns the initial checksum value
 */
static void
c_crc_crc32(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value string = GET_ARG(1);
  mrbc_int_t crc;
  if (argc < 2) {
    crc = 0;
  } else {
    crc = GET_INT_ARG(2);
  }
  if (string.tt == MRBC_TT_NIL) {
    SET_INT_RETURN(0);
    return;
  } else if (string.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "Expected a String type for the first argument");
    return;
  }
  uint32_t crc_value = generate_crc32((uint8_t *)string.string->data, (size_t)string.string->size, (uint32_t)crc);
  SET_INT_RETURN(crc_value);
}

/*
 * CRC.crc32_from_address(address, length, crc = nil) -> Integer
 */
static void
c_crc_crc32_from_address(mrbc_vm *vm, mrbc_value v[], int argc)
{
  uintptr_t address = (uintptr_t)GET_INT_ARG(1);
  if (address == 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Address must not be NULL");
    return;
  }
  mrbc_int_t length = GET_INT_ARG(2);
  mrbc_int_t crc;
  if (argc < 3) {
    crc = 0;
  } else {
    crc = GET_INT_ARG(3);
  }
  uint32_t crc_value = generate_crc32((uint8_t *)address, (size_t)length, (uint32_t)crc);
  SET_INT_RETURN(crc_value);
}

void
mrbc_crc_init(mrbc_vm *vm)
{
  mrbc_class *module_CRC = mrbc_define_module(vm, "CRC");

  mrbc_define_method(vm, module_CRC, "crc32", c_crc_crc32);
  mrbc_define_method(vm, module_CRC, "crc32_from_address", c_crc_crc32_from_address);
}
