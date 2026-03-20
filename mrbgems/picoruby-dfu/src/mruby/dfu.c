#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>

#ifdef DFU_HAS_ECDSA_KEY
#include "dfu_public_key.c.inc"

static mrb_value
mrb_s_ecdsa_public_key_pem(mrb_state *mrb, mrb_value self)
{
  return mrb_str_new_cstr(mrb, dfu_ecdsa_public_key_pem);
}
#endif

void
mrb_picoruby_dfu_gem_init(mrb_state* mrb)
{
#ifdef DFU_HAS_ECDSA_KEY
  struct RClass *module_DFU = mrb_define_module_id(mrb, MRB_SYM(DFU));
  mrb_define_class_method_id(mrb, module_DFU, MRB_SYM(ecdsa_public_key_pem),
    mrb_s_ecdsa_public_key_pem, MRB_ARGS_NONE());
#endif
}

void
mrb_picoruby_dfu_gem_final(mrb_state* mrb)
{
}
