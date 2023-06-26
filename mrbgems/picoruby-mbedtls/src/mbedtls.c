#include <stdbool.h>
#include <mrubyc.h>

#include "mbedtls/cmac.h"


static void
c__init_aes(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_cipher_context_t));
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)self.instance->data;
  mbedtls_cipher_init(ctx);
  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
  int ret;
  ret = mbedtls_cipher_setup(ctx, cipher_info);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setup failed");
  }
  mrbc_value key = GET_ARG(1);
  ret = mbedtls_cipher_cmac_starts(ctx, key.string->data, key.string->size * 8); /* last arg is keybits */
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_cmac_starts failed");
  }
  SET_RETURN(self);
}

static void
c_update(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;
  mrbc_value input = GET_ARG(1);
  int ret;
  ret = mbedtls_cipher_cmac_update(ctx, input.string->data, input.string->size);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_cmac_update failed");
  }
  SET_RETURN(*v);
}

static void
c_reset(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;
  int ret = mbedtls_cipher_cmac_reset(ctx);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_cmac_reset failed");
  }
  SET_RETURN(*v);
}

static void
c_digest(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;
  unsigned char output[16];
  int ret;
  ret = mbedtls_cipher_cmac_finish(ctx, output);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_cmac_finish failed");
  }
  mrbc_value digest = mrbc_string_new(vm, output, sizeof(output));
  SET_RETURN(digest);
}

void
mrbc_mbedtls_init(void)
{
  mrbc_class *class_MbedTLS = mrbc_define_class(0, "MbedTLS", mrbc_class_object);

  mrbc_value *v = mrbc_get_class_const(class_MbedTLS, mrbc_search_symid("CMAC"));
  mrbc_class *class_MbedTLS_CMAC = v->cls;

  mrbc_define_method(0, class_MbedTLS_CMAC, "_init_aes", c__init_aes);
  mrbc_define_method(0, class_MbedTLS_CMAC, "update", c_update);
  mrbc_define_method(0, class_MbedTLS_CMAC, "reset", c_reset);
  mrbc_define_method(0, class_MbedTLS_CMAC, "digest", c_digest);
}

