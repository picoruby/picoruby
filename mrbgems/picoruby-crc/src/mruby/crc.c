#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/string.h"

/*
 * CRC.crc32(string = nil, crc = nil) -> Integer
 * when string is nil, returns the initial checksum value
 */
static mrb_value
mrb_crc_s_crc32(mrb_state *mrb, mrb_value klass)
{
  mrb_value string = mrb_nil_value();
  mrb_int crc = 0;
  mrb_get_args(mrb, "|Si", &string, &crc);
  if (mrb_nil_p(string)) {
    return mrb_fixnum_value(0);
  }
  uint32_t crc_value = generate_crc32((uint8_t *)RSTRING_PTR(string), (size_t)RSTRING_LEN(string), (uint32_t)crc);
  return mrb_int_value(mrb, crc_value);
}

/*
 * CRC.crc32_from_address(address, length, crc = nil) -> Integer
 */
static mrb_value
mrb_crc_s_crc32_from_address(mrb_state *mrb, mrb_value klass)
{
  mrb_int address;
  mrb_int length;
  mrb_int crc = 0;
  mrb_get_args(mrb, "ii|i", &address, &length, &crc);
  if (address == 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "Address must not be NULL");
  }
  uint32_t crc_value = generate_crc32((uint8_t *)((uintptr_t)address), (size_t)length, (uint32_t)crc);
  return mrb_int_value(mrb, crc_value);
}


void
mrb_picoruby_crc_gem_init(mrb_state* mrb)
{
  struct RClass *module_CRC = mrb_define_module_id(mrb, MRB_SYM(CRC));

  mrb_define_class_method_id(mrb, module_CRC, MRB_SYM(crc32), mrb_crc_s_crc32, MRB_ARGS_OPT(2));
  mrb_define_class_method_id(mrb, module_CRC, MRB_SYM(crc32_from_address), mrb_crc_s_crc32_from_address, MRB_ARGS_ARG(2,1));
}

void
mrb_picoruby_crc_gem_final(mrb_state* mrb)
{
}
