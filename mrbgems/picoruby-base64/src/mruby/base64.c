#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/internal.h"

static mrb_value
mrb_base64_s_encode(mrb_state *mrb, mrb_value klass)
{
  size_t index = 0;
  size_t in_size, out_size;
  uint32_t n = 0;
  uint8_t n0, n1, n2, n3;
  const uint8_t *data;

  mrb_value input;
  mrb_get_args(mrb, "S", &input);
  data = (uint8_t *)RSTRING_PTR(input);

  in_size  = RSTRING_LEN(input);
  out_size = (in_size + 2) / 3 * 4 + 1;
  mrb_value output = mrb_str_new_capa(mrb, out_size);

  // create the main encoding
  for (index = 0; index < in_size; index += 3) {
    n = ((uint32_t)data[index]) << 16;
 
    if ((index + 1) < in_size)
      n += ((uint32_t)data[index + 1]) << 8;
 
    if ((index + 2) < in_size)
      n += data[index + 2];
 
    n0 = (uint8_t)(n >> 18) & 63;
    n1 = (uint8_t)(n >> 12) & 63;
    n2 = (uint8_t)(n >> 6)  & 63;
    n3 = (uint8_t)n         & 63;
 
    // generate encoded characters
    char encoded[5] = {
      base64chars[n0],
      base64chars[n1],
      (index + 1 < in_size) ? base64chars[n2] : '=',
      (index + 2 < in_size) ? base64chars[n3] : '=',
      '\0'
    };
    mrb_str_cat_cstr(mrb, output, encoded);
  }

  return output;
}

static mrb_value
mrb_base64_s_decode(mrb_state *mrb, mrb_value klass)
{
  size_t index = 0;
  size_t in_size, out_size;
  uint32_t buf = 0;
  int iter = 0;

  const uint8_t *data;

  mrb_value input;
  mrb_get_args(mrb, "S", &input);
  data = (uint8_t *)RSTRING_PTR(input);

  in_size  = RSTRING_LEN(input);
  out_size = (in_size + 3) / 4 * 3 + 1;
  mrb_value output = mrb_str_new_capa(mrb, out_size);

  /* Characters outside the alphabet (line breaks among them) are
   * skipped and '=' ends a group, so the output of encode64 and a
   * concatenation of encoded pieces both decode, as with CRuby. */
  for (; index <= in_size; index++) {
    unsigned char c = index < in_size ? data[index] : '=';
    if (c == '=') {
      if (iter == 3) {
        char decoded[2] = { (char)((buf >> 10) & 0xFF), (char)((buf >> 2) & 0xFF) };
        mrb_str_cat(mrb, output, decoded, 2);
      } else if (iter == 2) {
        char decoded[1] = { (char)((buf >> 4) & 0xFF) };
        mrb_str_cat(mrb, output, decoded, 1);
      }
      buf = 0;
      iter = 0;
      continue;
    }
    unsigned char v = base64_convert(c);
    if (v == 255) continue;
    buf = buf << 6 | v;
    iter++;
    if (iter == 4) {
      char decoded[3] = {
        (char)((buf >> 16) & 0xFF),
        (char)((buf >> 8) & 0xFF),
        (char)(buf & 0xFF)
      };
      mrb_str_cat(mrb, output, decoded, 3);
      buf = 0;
      iter = 0;
    }
  }

  /* bytes, as CRuby's ASCII-8BIT result is: an index into a decoded
     random token counts bytes, not the UTF-8 characters they might form */
  RSTR_ENCODING_SET(mrb_str_ptr(output), MRB_STR_ENCODING_BINARY);
  return output;
}


void
mrb_picoruby_base64_gem_init(mrb_state* mrb)
{
  struct RClass *class_Base64 = mrb_define_class_id(mrb, MRB_SYM(Base64), mrb->object_class);

  mrb_define_class_method_id(mrb, class_Base64, MRB_SYM(encode64), mrb_base64_s_encode, MRB_ARGS_REQ(1));
  /* the plain encoding with no line breaks; mrblib's encode64 adds them */
  mrb_define_class_method_id(mrb, class_Base64, MRB_SYM(strict_encode64), mrb_base64_s_encode, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Base64, MRB_SYM(decode64), mrb_base64_s_decode, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_base64_gem_final(mrb_state* mrb)
{
}
