#include "mruby.h"
#include "mruby/numeric.h"
#include "mruby/presym.h"

static mrb_value
mrb_s_random_int(mrb_state *mrb, mrb_value klass)
{
  uint8_t bytes[4];
  uint32_t ret = 0;

  /* The failure-reporting call, not the byte one: a source that never
     became ready would otherwise hand back zeros and this would return
     0 as if it were a draw. */
  if (rng_random_bytes_impl(bytes, sizeof(bytes)) != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "entropy source failed");
  }
  for (int i = 0; i < 4; i++) {
    ret = (ret << 8) | bytes[i];
  }
  /* Keep the full 0..2**32-1 range on a 32-bit mrb_int as well */
  return mrb_uint64_value(mrb, ret);
}

static mrb_value
mrb_s_random_string(mrb_state *mrb, mrb_value klass)
{
  mrb_int len;
  mrb_get_args(mrb, "i", &len);
  unsigned char *buf = (unsigned char *)mrb_malloc(mrb, len);
  if (rng_random_bytes_impl((uint8_t *)buf, (size_t)len) != 0) {
    mrb_free(mrb, buf);
    mrb_raise(mrb, E_RUNTIME_ERROR, "entropy source failed");
  }
  mrb_value ret = mrb_str_new(mrb, (const char *)buf, len);
  mrb_free(mrb, buf);
  return ret;
}

void
mrb_picoruby_rng_gem_init(mrb_state* mrb)
{
  struct RClass *module_RNG = mrb_define_module_id(mrb, MRB_SYM(RNG));

  mrb_define_class_method_id(mrb, module_RNG, MRB_SYM(random_int), mrb_s_random_int, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, module_RNG, MRB_SYM(random_string), mrb_s_random_string, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_rng_gem_final(mrb_state* mrb)
{
}
