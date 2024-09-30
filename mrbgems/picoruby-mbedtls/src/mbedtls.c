#include <stdbool.h>
#include <mrubyc.h>

#include "cmac.h"
#include "cipher.h"
#include "digest.h"

void
mrbc_mbedtls_init(mrbc_vm *vm)
{
  mrbc_class *class_MbedTLS = mrbc_define_class(vm, "MbedTLS", mrbc_class_object);

  gem_mbedtls_cmac_init(vm, class_MbedTLS);
  gem_mbedtls_cipher_init(vm, class_MbedTLS);
  gem_mbedtls_digest_init(vm, class_MbedTLS);
}

