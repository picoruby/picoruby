#include <mrubyc.h>

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
  mbedtls_cipher_type_t cipher_type;
  uint8_t key_len, iv_len;
  mbedtls_cipher_type_key_iv_len(cipher_name, &cipher_type, &key_len, &iv_len);
  if (cipher_type == MBEDTLS_CIPHER_NONE) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "unsupported cipher suite");
    return;
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(cipher_instance_t));
  cipher_instance_t *instance_data = (cipher_instance_t *)self.instance->data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;
  mbedtls_cipher_init(ctx);
  instance_data->cipher_type = cipher_type;
  instance_data->operation = MBEDTLS_OPERATION_NONE;
  instance_data->key_len = key_len;
  instance_data->iv_len = iv_len;
  instance_data->key_set = false;
  instance_data->iv_set = false;

  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(cipher_type);
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
  SET_RETURN(self);
}

static void
c_mbedtls_cipher_ciphers(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value ret = mrbc_array_new(vm, CIPHER_SUITES_COUNT);
  for (int i = 0; i < CIPHER_SUITES_COUNT; i++) {
    mrbc_value str = mrbc_string_new_cstr(vm, cipher_suites[i].name);
    mrbc_array_set(&ret, i, &str);
  }
  SET_RETURN(ret);
}

static void
c_mbedtls_cipher_encrypt(mrbc_vm *vm, mrbc_value *v, int argc)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  instance_data->operation = MBEDTLS_ENCRYPT;
}

static void
c_mbedtls_cipher_decrypt(mrbc_vm *vm, mrbc_value *v, int argc)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  instance_data->operation = MBEDTLS_DECRYPT;
}

static void
c_mbedtls_cipher_key_len(mrbc_vm *vm, mrbc_value *v, int argc)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  SET_INT_RETURN(instance_data->key_len);
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

  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  if (instance_data->key_set) {
    console_printf("[WARN] key should be set once per instance, ignoring\n");
    return;
  }
  if (instance_data->operation == MBEDTLS_OPERATION_NONE) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "operation is not set");
    return;
  }
  if (key.string->size != instance_data->key_len) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "key length is invalid");
    return;
  }

  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  int ret;
  ret = mbedtls_cipher_setkey(ctx, key.string->data, key.string->size * 8, instance_data->operation); /* last arg is keybits */
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_setkey failed");
    return;
  }
  if (mbedtls_cipher_is_cbc(instance_data->cipher_type)) {
    ret = mbedtls_cipher_set_padding_mode(ctx, MBEDTLS_PADDING_PKCS7);
    if (ret != 0) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_cipher_set_padding_mode failed");
      return;
    }
  }

  instance_data->key_set = true;
  mrbc_incref(&v[0]);
  mrbc_incref(&key);
  SET_RETURN(key);
}

static void
c_mbedtls_cipher_iv_len(mrbc_vm *vm, mrbc_value *v, int argc)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  SET_INT_RETURN(instance_data->iv_len);
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

  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  if (instance_data->iv_set) {
    console_printf("[WARN] iv should be set once per instance, ignoring\n");
    return;
  }
  if (instance_data->operation == MBEDTLS_OPERATION_NONE) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "operation is not set");
    return;
  }
  if (iv.string->size != instance_data->iv_len) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "iv length is invalid");
    return;
  }

  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

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

  instance_data->iv_set = true;
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

  int ret;
  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

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

  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

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

  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

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
                                                //
  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

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
  cipher_instance_t *instance_data = (cipher_instance_t *)v->instance->data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

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
