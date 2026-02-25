#include <mrubyc.h>

#ifdef OTA_HAS_ECDSA_KEY
#include "ota_public_key.c.inc"

static void
c_ecdsa_public_key_pem(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value str = mrbc_string_new_cstr(vm, ota_ecdsa_public_key_pem);
  SET_RETURN(str);
}
#endif

void
mrbc_ota_init(mrbc_vm *vm)
{
#ifdef OTA_HAS_ECDSA_KEY
  mrbc_class *module_OTA = mrbc_define_module(vm, "OTA");
  mrbc_define_method(vm, module_OTA, "ecdsa_public_key_pem", c_ecdsa_public_key_pem);
#endif
}
