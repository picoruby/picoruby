#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/internal.h"

static mrb_value
base64_encode_value(mrb_state *mrb, size_t line_len)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  size_t len = (size_t)RSTRING_LEN(input);
  mrb_value output = mrb_str_new_capa(mrb, (mrb_int)base64_encoded_size(len, line_len));
  size_t size = base64_encode((const uint8_t *)RSTRING_PTR(input), len,
                              (uint8_t *)RSTRING_PTR(output), line_len);
  mrb_str_resize(mrb, output, (mrb_int)size);
  return output;
}

static mrb_value
base64_decode_value(mrb_state *mrb, int strict)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  size_t len = (size_t)RSTRING_LEN(input);
  /* the capa has room for the NUL already */
  mrb_value output = mrb_str_new_capa(mrb, (mrb_int)base64_decoded_capa(len) - 1);
  size_t size;
  if (base64_decode((const uint8_t *)RSTRING_PTR(input), len,
                    (uint8_t *)RSTRING_PTR(output), &size, strict) < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid base64");
  }
  mrb_str_resize(mrb, output, (mrb_int)size);
  /* bytes, as CRuby's ASCII-8BIT result is: an index into a decoded
     random token counts bytes, not the UTF-8 characters they might form */
  RSTR_ENCODING_SET(mrb_str_ptr(output), MRB_STR_ENCODING_BINARY);
  return output;
}

/* RFC 2045: a newline after every 60 characters and at the end */
static mrb_value
mrb_base64_s_encode64(mrb_state *mrb, mrb_value klass)
{
  return base64_encode_value(mrb, BASE64_LINE_LEN);
}

static mrb_value
mrb_base64_s_strict_encode64(mrb_state *mrb, mrb_value klass)
{
  return base64_encode_value(mrb, 0);
}

static mrb_value
mrb_base64_s_decode64(mrb_state *mrb, mrb_value klass)
{
  return base64_decode_value(mrb, 0);
}

static mrb_value
mrb_base64_s_strict_decode64(mrb_state *mrb, mrb_value klass)
{
  return base64_decode_value(mrb, 1);
}

void
mrb_picoruby_base64_gem_init(mrb_state* mrb)
{
  struct RClass *class_Base64 = mrb_define_class_id(mrb, MRB_SYM(Base64), mrb->object_class);

  mrb_define_class_method_id(mrb, class_Base64, MRB_SYM(encode64), mrb_base64_s_encode64, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Base64, MRB_SYM(strict_encode64), mrb_base64_s_strict_encode64, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Base64, MRB_SYM(decode64), mrb_base64_s_decode64, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Base64, MRB_SYM(strict_decode64), mrb_base64_s_strict_decode64, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_base64_gem_final(mrb_state* mrb)
{
}
