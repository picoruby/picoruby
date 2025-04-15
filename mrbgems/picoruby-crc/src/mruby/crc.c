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

void
mrb_picoruby_crc_gem_init(mrb_state* mrb)
{
  struct RClass *module_CRC = mrb_define_module_id(mrb, MRB_SYM(CRC));

  mrb_define_class_method_id(mrb, module_CRC, MRB_SYM(crc32), mrb_crc_s_crc32, MRB_ARGS_OPT(2));
}

void
mrb_picoruby_crc_gem_final(mrb_state* mrb)
{
}
