#include "mrubyc.h"

#include "mbedtls.h"

void
mrbc_mbedtls_init(mrbc_vm *vm)
{
  mrbc_class *module_MbedTLS = mrbc_define_module(vm, "MbedTLS");

  gem_mbedtls_cmac_init(vm, module_MbedTLS);
  gem_mbedtls_hmac_init(vm, module_MbedTLS);
  gem_mbedtls_cipher_init(vm, module_MbedTLS);
  gem_mbedtls_digest_init(vm, module_MbedTLS);
  gem_mbedtls_pkey_init(vm, module_MbedTLS);
}

