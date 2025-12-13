#include "mruby.h"
#include "mruby/presym.h"

static mrb_value
mrb_s_random_int(mrb_state *mrb, mrb_value klass)
{
  uint32_t ret = 0;
  for (int i = 0; i < 4; i++) {
    ret = (ret << 8) | rng_random_byte_impl();
  }
  return mrb_fixnum_value(ret);
}

static mrb_value
mrb_s_random_string(mrb_state *mrb, mrb_value klass)
{
  mrb_int len;
  mrb_get_args(mrb, "i", &len);
  unsigned char *buf = (unsigned char *)mrb_malloc(mrb, len);
  for (int i = 0; i < len; i++) {
    buf[i] = (unsigned char)rng_random_byte_impl();
  }
  mrb_value ret = mrb_str_new(mrb, (const char *)buf, len);
  mrb_free(mrb, buf);
  return ret;
}

void
mrb_picoruby_rng_gem_init(mrb_state* mrb)
{
  struct RClass *class_RNG = mrb_define_class_id(mrb, MRB_SYM(RNG), mrb->object_class);

  mrb_define_class_method_id(mrb, class_RNG, MRB_SYM(random_int), mrb_s_random_int, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_RNG, MRB_SYM(random_string), mrb_s_random_string, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_rng_gem_final(mrb_state* mrb)
{
}
