#include <mrubyc.h>
#include "mbedtls/cipher.h"
#include "mbedtls/aes.h"

mbedtls_cipher_type_t
c_mbedtls_cipher_cipher_name(int key)
{
  mbedtls_cipher_type_t ret;
  switch (key) {
    case 0x0001:
      ret = MBEDTLS_CIPHER_AES_128_CBC;
      break;
    case 0x0002:
      ret = MBEDTLS_CIPHER_AES_192_CBC;
      break;
    case 0x0003:
      ret = MBEDTLS_CIPHER_AES_256_CBC;
      break;
    // case 0x1001:
    //   ret = MBEDTLS_CIPHER_AES_128_GCM;
    //   break;
    // case 0x1002:
    //   ret = MBEDTLS_CIPHER_AES_192_GCM;
    //   break;
    // case 0x1003:
    //   ret = MBEDTLS_CIPHER_AES_256_GCM;
    //   break;
    default:
      ret = MBEDTLS_CIPHER_NONE;
  }
  return ret;
}

mbedtls_operation_t
c_mbedtls_cipher_operation_name(int key)
{
  mbedtls_operation_t ret;
  switch (key) {
    case 0:
      ret = MBEDTLS_ENCRYPT;
      break;
    case 1:
      ret = MBEDTLS_DECRYPT;
      break;
    default:
      ret = MBEDTLS_OPERATION_NONE;
  }
  return ret;
}

static void
c_mbedtls_cipher__init_ctx(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_cipher_context_t));
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)self.instance->data;
  mbedtls_cipher_init(ctx);

  mrbc_value cipher_suite = GET_ARG(1);
  mrbc_value key          = GET_ARG(2);
  mrbc_value operation    = GET_ARG(3);

  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(c_mbedtls_cipher_cipher_name(mrbc_integer(cipher_suite)));
  int ret;
  ret = mbedtls_cipher_setup(ctx, cipher_info);
  if (ret != 0) {
    if (ret == MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setup failed (bad input data)");
    } else {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setup failed");
    }
    return;
  }
  ret = mbedtls_cipher_setkey(ctx, key.string->data, key.string->size * 8, c_mbedtls_cipher_operation_name(mrbc_integer(operation))); /* last arg is keybits */
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setkey failed");
    return;
  }
  ret = mbedtls_cipher_set_padding_mode(ctx, MBEDTLS_PADDING_PKCS7);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_set_padding_mode failed");
    return;
  }

  SET_RETURN(self);
}

static void
c_mbedtls_cipher__set_iv(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value iv = GET_ARG(1);
  if (iv.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;
  int ret;
  ret = mbedtls_cipher_set_iv(ctx, iv.string->data, iv.string->size);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_set_iv failed");
    return;
  }
  ret = mbedtls_cipher_reset(ctx);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_reset failed");
    return;
  }

  SET_RETURN(*v);
}

static void
c_mbedtls_cipher_update(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  unsigned char* output = mrbc_alloc(vm, input.string->size + 16); // need at least input length + block size
  size_t out_len = input.string->size + 16;
  int ret;

  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;

  ret = mbedtls_cipher_update(ctx, input.string->data, input.string->size, output, &out_len);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_reset failed");
    return;
  }
  mrbc_value ret_value = mrbc_string_new(vm, output, out_len);
  mrbc_free(vm, output);
  mrbc_incref(&v[0]);
  SET_RETURN(ret_value);
}

static void
c_mbedtls_cipher_finish(mrbc_vm *vm, mrbc_value *v, int argc)
{
  unsigned char* output = mrbc_alloc(vm, 16); // need at least block size
  size_t out_len = 16;
  int ret;

  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;
  ret = mbedtls_cipher_finish(ctx, output, &out_len);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_reset failed");
    return;
  }
  mrbc_value ret_value = mrbc_string_new(vm, output, out_len);
  mrbc_free(vm, output);
  mrbc_incref(&v[0]);
  SET_RETURN(ret_value);
}

void
gem_mbedtls_cipher_init(void)
{
  mrbc_class *class_MbedTLS = mrbc_define_class(0, "MbedTLS", mrbc_class_object);

  mrbc_value *Cipher = mrbc_get_class_const(class_MbedTLS, mrbc_search_symid("Cipher"));
  mrbc_class *class_MbedTLS_Cipher = Cipher->cls;

  mrbc_define_method(0, class_MbedTLS_Cipher, "_init_ctx", c_mbedtls_cipher__init_ctx);
  mrbc_define_method(0, class_MbedTLS_Cipher, "_set_iv",   c_mbedtls_cipher__set_iv);
  mrbc_define_method(0, class_MbedTLS_Cipher, "update",    c_mbedtls_cipher_update);
  mrbc_define_method(0, class_MbedTLS_Cipher, "finish",    c_mbedtls_cipher_finish);

  mrbc_define_method(0, class_MbedTLS_Cipher, "unprocessed", c_mbedtls_cipher_unprocessed);

  /*
  mrbc_define_method(0, class_MbedTLS_CMAC, "_init_aes", c__init_aes);
  mrbc_define_method(0, class_MbedTLS_CMAC, "update", c_update);
  mrbc_define_method(0, class_MbedTLS_CMAC, "reset", c_reset);
  mrbc_define_method(0, class_MbedTLS_CMAC, "digest", c_digest);
  */
}
