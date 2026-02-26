#include <mrubyc.h>

#ifdef DFU_HAS_ECDSA_KEY
#include "dfu_public_key.c.inc"

static void
c_ecdsa_public_key_pem(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value str = mrbc_string_new_cstr(vm, dfu_ecdsa_public_key_pem);
  SET_RETURN(str);
}
#endif

void
mrbc_dfu_init(mrbc_vm *vm)
{
#ifdef DFU_HAS_ECDSA_KEY
  mrbc_class *module_DFU = mrbc_define_module(vm, "DFU");
  mrbc_define_method(vm, module_DFU, "ecdsa_public_key_pem", c_ecdsa_public_key_pem);
#endif
}
