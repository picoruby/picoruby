#include "mruby.h"
#include "mruby/presym.h"

#include "mbedtls.h"

void
mrb_picoruby_mbedtls_gem_init(mrb_state* mrb)
{
  struct RClass *module_MbedTLS = mrb_define_module_id(mrb, MRB_SYM(MbedTLS));

  gem_mbedtls_cmac_init(mrb, module_MbedTLS);
  gem_mbedtls_hmac_init(mrb, module_MbedTLS);
  gem_mbedtls_cipher_init(mrb, module_MbedTLS);
  gem_mbedtls_digest_init(mrb, module_MbedTLS);
  gem_mbedtls_pkey_init(mrb, module_MbedTLS);
}

void
mrb_picoruby_mbedtls_gem_final(mrb_state* mrb)
{
}
