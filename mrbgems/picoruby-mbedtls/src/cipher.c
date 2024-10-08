#include <mrubyc.h>
#include "mbedtls/cipher.h"
#include "mbedtls/aes.h"

static mbedtls_cipher_type_t
mbedtls_cipher_cipher_name(int key)
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
    case 0x1001:
      ret = MBEDTLS_CIPHER_AES_128_GCM;
      break;
    case 0x1002:
      ret = MBEDTLS_CIPHER_AES_192_GCM;
      break;
    case 0x1003:
      ret = MBEDTLS_CIPHER_AES_256_GCM;
      break;
    default:
      ret = MBEDTLS_CIPHER_NONE;
  }
  return ret;
}

static mbedtls_operation_t
mbedtls_cipher_operation_name(int key)
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

static int
mbedtls_cipher_is_cbc(int cipher_key)
{
  return cipher_key < 0x0010;
}

static void
c_mbedtls_cipher__init_ctx(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_cipher_context_t));
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)self.instance->data;
  mbedtls_cipher_init(ctx);

  int cipher_suite = GET_INT_ARG(1);
  mrbc_instance_setiv(&self, mrbc_str_to_symid("cipher_key"), &GET_ARG(1));
  mrbc_value key   = GET_ARG(2);
  int operation    = GET_INT_ARG(3);

  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(mbedtls_cipher_cipher_name(cipher_suite));
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
  ret = mbedtls_cipher_setkey(ctx, key.string->data, key.string->size * 8, mbedtls_cipher_operation_name(operation)); /* last arg is keybits */
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setkey failed");
    return;
  }
  if (mbedtls_cipher_is_cbc(cipher_suite)) {
    ret = mbedtls_cipher_set_padding_mode(ctx, MBEDTLS_PADDING_PKCS7);
    if (ret != 0) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_set_padding_mode failed");
      return;
    }
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
c_mbedtls_cipher_update_ad(mrbc_vm *vm, mrbc_value *v, int argc)
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

  int ret;
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;

  ret = mbedtls_cipher_update_ad(ctx, input.string->data, input.string->size);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_update_ad failed");
    return;
  }

  mrbc_incref(&v[0]);
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
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_update failed");
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
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_finish failed");
    return;
  }
  mrbc_value ret_value = mrbc_string_new(vm, output, out_len);
  mrbc_free(vm, output);
  mrbc_incref(&v[0]);
  SET_RETURN(ret_value);
}

static void
c_mbedtls_cipher_write_tag(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int ret;
  size_t tag_len = 16;
  unsigned char* tag = mrbc_alloc(vm, tag_len); // tag size is 16 for AES-*-GCM and ChaCha20-Poly1305
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;

  ret = mbedtls_cipher_write_tag(ctx, tag, tag_len);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_write_tag failed");
    return;
  }

  mrbc_value ret_value = mrbc_string_new(vm, tag, tag_len);
  mrbc_free(vm, tag);
  mrbc_incref(&v[0]);
  SET_RETURN(ret_value);
}

static void
c_mbedtls_cipher_check_tag(mrbc_vm *vm, mrbc_value *v, int argc)
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

  int ret;
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)v->instance->data;

  ret = mbedtls_cipher_check_tag(ctx, input.string->data, input.string->size);
  if (ret == MBEDTLS_ERR_CIPHER_AUTH_FAILED) {
    mrbc_incref(&v[0]);
    SET_BOOL_RETURN(MRBC_TT_FALSE);
  } else if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_check_tag failed");
    return;
  } else {
    mrbc_incref(&v[0]);
    SET_BOOL_RETURN(MRBC_TT_TRUE);
  }
}

void
gem_mbedtls_cipher_init(mrbc_vm *vm, mrbc_class *module_MbedTLS)
{
  mrbc_class *class_MbedTLS_Cipher = mrbc_define_class_under(vm, module_MbedTLS, "Cipher", mrbc_class_object);

  mrbc_define_method(vm, class_MbedTLS_Cipher, "_init_ctx", c_mbedtls_cipher__init_ctx);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "_set_iv",   c_mbedtls_cipher__set_iv);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "update",    c_mbedtls_cipher_update);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "finish",    c_mbedtls_cipher_finish);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "update_ad", c_mbedtls_cipher_update_ad);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "write_tag", c_mbedtls_cipher_write_tag);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "check_tag", c_mbedtls_cipher_check_tag);
}
