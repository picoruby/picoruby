#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"

static mrb_value
mrb_base16_s_encode(mrb_state *mrb, mrb_value klass)
{
  size_t index = 0;
  size_t in_size, out_size;
  uint8_t n, n0, n1;
  const uint8_t *data;

  mrb_value input;
  mrb_get_args(mrb, "S", &input);
  data = (uint8_t *)RSTRING_PTR(input);

  in_size  = RSTRING_LEN(input);
  out_size = in_size * 2;
  mrb_value output = mrb_str_new_capa(mrb, out_size);

  for (index = 0; index < in_size; index++) {
    n = data[index];
    n0 = (n >> 4) & 15;
    n1 = n & 15;
    mrb_str_cat_cstr(mrb, output, (char[]){base16chars[n0], base16chars[n1], '\0'});
  }

  return output;
}

static mrb_value
mrb_base16_s_decode(mrb_state *mrb, mrb_value klass)
{
  size_t index = 0;
  size_t in_size, out_size;
  uint8_t n0, n1;
  const uint8_t *data;

  mrb_value input;
  mrb_get_args(mrb, "S", &input);
  data = (uint8_t *)RSTRING_PTR(input);

  in_size  = RSTRING_LEN(input);
  out_size = in_size / 2;
  // Base16 in RFC4648 assumes input size to be even,
  // but Ruby's pack('H*') treats as if there is a 0 at the end
  if (in_size % 2 == 1) {
    out_size += 1;
  }
  mrb_value output = mrb_str_new_capa(mrb, out_size);

  for (index = 0; index < out_size; index++) {
    n0 = base16_convert(data[index * 2]);
    if (index * 2 + 1 < in_size) {
      n1 = base16_convert(data[index * 2 + 1]);
    } else {
      n1 = 0;
    }
    mrb_str_cat_cstr(mrb, output, (char[]){(n0 << 4) | n1, '\0'});
  }

  return output;
}

void
mrb_picoruby_base16_gem_init(mrb_state* mrb)
{
  struct RClass *class_Base16 = mrb_define_class_id(mrb, MRB_SYM(Base16), mrb->object_class);

  mrb_define_class_method_id(mrb, class_Base16, MRB_SYM(encode16), mrb_base16_s_encode, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Base16, MRB_SYM(decode16), mrb_base16_s_decode, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_base16_gem_final(mrb_state* mrb)
{
}
