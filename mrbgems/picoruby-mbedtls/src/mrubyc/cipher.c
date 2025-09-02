#include <stdio.h>
#include "mrubyc.h"

static void
c_mbedtls_cipher_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  const char *cipher_name;
  mrbc_value cipher_suite = GET_ARG(1);
  switch (cipher_suite.tt) {
    case MRBC_TT_STRING:
      cipher_name = (char *)cipher_suite.string->data;
      break;
    default:
      mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
      return;
  }
  int cipher_type;
  uint8_t key_len, iv_len;
  console_printf("Cipher.new: cipher_name='%s'\n", cipher_name);
  Mbedtls_cipher_type_key_iv_len(cipher_name, &cipher_type, &key_len, &iv_len);
  console_printf("Cipher.new: cipher_type=%d, key_len=%d, iv_len=%d\n", cipher_type, key_len, iv_len);
  if (cipher_type == 0) {
    console_printf("Cipher.new: unsupported cipher suite '%s'\n", cipher_name);
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "unsupported cipher suite");
    return;
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, MbedTLS_cipher_instance_size());
  int ret = MbedTLS_cipher_new(self.instance->data, cipher_type, key_len, iv_len);
  if (ret == CIPHER_BAD_INPUT_DATA) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setup failed (bad input data)");
    return;
  } else if (ret == CIPHER_FAILED_TO_SETUP) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setup failed");
    return;
  }
  SET_RETURN(self);
}

static void
c_mbedtls_cipher_ciphers(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value ret = mrbc_array_new(vm, CIPHER_SUITES_COUNT);
  for (int i = 0; i < CIPHER_SUITES_COUNT; i++) {
    const char *cipher_name = MbedTLS_cipher_cipher_name(i);
    mrbc_value str = mrbc_string_new_cstr(vm, cipher_name);
    mrbc_array_set(&ret, i, &str);
  }
  SET_RETURN(ret);
}

static void
c_mbedtls_cipher_encrypt(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *cipher_instance = v->instance->data;
  MbedTLS_cipher_set_encrypt_to_operation(cipher_instance);
}

static void
c_mbedtls_cipher_decrypt(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *cipher_instance = v->instance->data;
  MbedTLS_cipher_set_decrypt_to_operation(cipher_instance);
}

static void
c_mbedtls_cipher_key_len(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *cipher_instance = v->instance->data;
  SET_INT_RETURN(MbedTLS_cipher_get_key_len(cipher_instance));
}

static void
c_mbedtls_cipher_key_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value key = GET_ARG(1);
  if (key.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  uint8_t *cipher_instance = v->instance->data;

  int ret = MbedTLS_cipher_set_key(cipher_instance, (const uint8_t *)key.string->data, key.string->size * 8);

  if (ret == CIPHER_ALREADY_SET) {
    console_printf("[WARN] key should be set once per instance, ignoring\n");
  } else if (ret == CIPHER_OPERATION_NOT_SET) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "operation is not set");
  } else if (ret == CIPHER_INVALID_LENGTH) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "key length is invalid");
  } else if (ret != CIPHER_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setkey failed");
  }

  mrbc_incref(&v[0]);
  mrbc_incref(&key);
  SET_RETURN(key);
}

static void
c_mbedtls_cipher_iv_len(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *cipher_instance = v->instance->data;
  SET_INT_RETURN(MbedTLS_cipher_get_iv_len(cipher_instance));
}

static void
c_mbedtls_cipher_iv_eq(mrbc_vm *vm, mrbc_value *v, int argc)
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

  uint8_t *cipher_instance = v->instance->data;

  int ret = MbedTLS_cipher_set_iv(cipher_instance, (const uint8_t *)iv.string->data, iv.string->size);

  if (ret == CIPHER_ALREADY_SET) {
    console_printf("[WARN] iv should be set once per instance, ignoring\n");
  } else if (ret == CIPHER_OPERATION_NOT_SET) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "operation is not set");
  } else if (ret == CIPHER_INVALID_LENGTH) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "iv length is invalid");
  } else if (ret != CIPHER_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_set_iv failed");
  }

  mrbc_incref(&v[0]);
  mrbc_incref(&iv);
  SET_RETURN(iv);
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

  uint8_t *cipher_instance = v->instance->data;

  if (MbedTLS_cipher_update_ad(cipher_instance, (const uint8_t *)input.string->data, input.string->size) != CIPHER_SUCCESS) {
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

  size_t out_len = input.string->size + 16;
  unsigned char* output = mrbc_alloc(vm, out_len);

  uint8_t *cipher_instance = v->instance->data;

  if (MbedTLS_cipher_update(cipher_instance, (const uint8_t *)input.string->data, input.string->size, output, &out_len) != CIPHER_SUCCESS) {
    mrbc_free(vm, output);
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
  size_t out_len = 16;
  unsigned char* output = mrbc_alloc(vm, out_len);
  int mbedtls_err;

  uint8_t *cipher_instance = v->instance->data;

  if (MbedTLS_cipher_finish(cipher_instance, output, &out_len, &mbedtls_err) != CIPHER_SUCCESS) {
    char error_buf[200];
    MbedTLS_strerror(mbedtls_err, error_buf, 100);
    snprintf(error_buf + strlen(error_buf), 100, " (error code: %d)", mbedtls_err);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), error_buf);
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
  size_t tag_len = 16;
  unsigned char* tag = mrbc_alloc(vm, tag_len);
  uint8_t *cipher_instance = v->instance->data;

  int ret = MbedTLS_cipher_write_tag(cipher_instance, tag, &tag_len);

  if (ret == CIPHER_NOT_AEAD) {
    mrbc_free(vm, tag);
    mrbc_value ret_value = mrbc_string_new_cstr(vm, "");
    SET_RETURN(ret_value);
    return;
  }
  if (ret != CIPHER_SUCCESS) {
    mrbc_free(vm, tag);
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

  uint8_t *cipher_instance = v->instance->data;

  int ret = MbedTLS_cipher_check_tag(cipher_instance, (const unsigned char *)input.string->data, input.string->size);

  if (ret == CIPHER_AUTH_FAILED) {
    SET_BOOL_RETURN(MRBC_TT_FALSE);
  } else if (ret != CIPHER_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_check_tag failed");
  } else {
    SET_BOOL_RETURN(MRBC_TT_TRUE);
  }
}

void
gem_mbedtls_cipher_init(mrbc_vm *vm, mrbc_class *module_MbedTLS)
{
  mrbc_class *class_MbedTLS_Cipher = mrbc_define_class_under(vm, module_MbedTLS, "Cipher", mrbc_class_object);

  mrbc_define_method(vm, class_MbedTLS_Cipher, "new",       c_mbedtls_cipher_new);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "ciphers",   c_mbedtls_cipher_ciphers);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "encrypt",   c_mbedtls_cipher_encrypt);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "decrypt",   c_mbedtls_cipher_decrypt);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "key_len",   c_mbedtls_cipher_key_len);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "key=",      c_mbedtls_cipher_key_eq);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "iv_len",    c_mbedtls_cipher_iv_len);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "iv=",       c_mbedtls_cipher_iv_eq);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "update",    c_mbedtls_cipher_update);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "update_ad", c_mbedtls_cipher_update_ad);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "finish",    c_mbedtls_cipher_finish);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "write_tag", c_mbedtls_cipher_write_tag);
  mrbc_define_method(vm, class_MbedTLS_Cipher, "check_tag", c_mbedtls_cipher_check_tag);
}
