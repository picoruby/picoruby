#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/string.h>

#ifdef OTA_HAS_ECDSA_KEY
#include "ota_public_key.c.inc"

static mrb_value
mrb_s_ecdsa_public_key_pem(mrb_state *mrb, mrb_value self)
{
  return mrb_str_new_cstr(mrb, ota_ecdsa_public_key_pem);
}
#endif

void
mrb_picoruby_ota_gem_init(mrb_state* mrb)
{
#ifdef OTA_HAS_ECDSA_KEY
  struct RClass *module_OTA = mrb_define_module_id(mrb, MRB_SYM(OTA));
  mrb_define_class_method_id(mrb, module_OTA, MRB_SYM(ecdsa_public_key_pem),
    mrb_s_ecdsa_public_key_pem, MRB_ARGS_NONE());
#endif
}

void
mrb_picoruby_ota_gem_final(mrb_state* mrb)
{
}
