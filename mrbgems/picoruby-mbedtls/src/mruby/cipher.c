#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/data.h"
#include "mruby/class.h"
#include "mbedtls/error.h"

static void
mrb_cipher_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_cipher_type = {
  "Cipher", mrb_cipher_free,
};

static mrb_value
mrb_mbedtls_cipher_initialize(mrb_state *mrb, mrb_value self)
{
  const char *cipher_name;
  mrb_get_args(mrb, "z", &cipher_name);

  mbedtls_cipher_type_t cipher_type;
  uint8_t key_len, iv_len;
#ifdef PICORUBY_DEBUG
  printf("Cipher.new: cipher_name='%s'\n", cipher_name);
#endif
  mbedtls_cipher_type_key_iv_len(cipher_name, &cipher_type, &key_len, &iv_len);
#ifdef PICORUBY_DEBUG
  printf("Cipher.new: cipher_type=%d, key_len=%d, iv_len=%d\n", cipher_type, key_len, iv_len);
#endif
  if (cipher_type == MBEDTLS_CIPHER_NONE) {
#ifdef PICORUBY_DEBUG
    printf("Cipher.new: unsupported cipher suite '%s'\n", cipher_name);
#endif
    mrb_raise(mrb, E_ARGUMENT_ERROR, "unsupported cipher suite");
  }

  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_malloc(mrb, sizeof(cipher_instance_t));
  DATA_PTR(self) = instance_data;
  DATA_TYPE(self) = &mrb_cipher_type;
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
      mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setup failed (bad input data)");
    } else {
      mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setup failed");
    }
  }
  return self;
}

static mrb_value
mrb_mbedtls_cipher_ciphers(mrb_state *mrb, mrb_value self)
{
  mrb_value ret = mrb_ary_new_capa(mrb, CIPHER_SUITES_COUNT);
  for (int i = 0; i < CIPHER_SUITES_COUNT; i++) {
    mrb_value str = mrb_str_new_cstr(mrb, cipher_suites[i].name);
    mrb_ary_set(mrb, ret, i, str);
  }
  return ret;
}

static mrb_value
mrb_mbedtls_cipher_encrypt(mrb_state *mrb, mrb_value self)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  instance_data->operation = MBEDTLS_ENCRYPT;
  return self;
}

static mrb_value
mrb_mbedtls_cipher_decrypt(mrb_state *mrb, mrb_value self)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  instance_data->operation = MBEDTLS_DECRYPT;
  return self;
}

static mrb_value
mrb_mbedtls_cipher_key_len(mrb_state *mrb, mrb_value self)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  return mrb_fixnum_value(instance_data->key_len);
}

static mrb_value
mrb_mbedtls_cipher_key_eq(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_get_args(mrb, "S", &key);

  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  if (instance_data->key_set) {
    mrb_warn(mrb, "key should be set once per instance, ignoring\n");
    return key;
  }
  if (instance_data->operation == MBEDTLS_OPERATION_NONE) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "operation is not set");
  }
  if (RSTRING_LEN(key) != instance_data->key_len) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "key length is invalid");
  }

  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  int ret;
  ret = mbedtls_cipher_setkey(ctx, (const unsigned char *)RSTRING_PTR(key), RSTRING_LEN(key) * 8, instance_data->operation); /* last arg is keybits */
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setkey failed");
  }
  if (mbedtls_cipher_is_cbc(instance_data->cipher_type)) {
    ret = mbedtls_cipher_set_padding_mode(ctx, MBEDTLS_PADDING_PKCS7);
    if (ret != 0) {
      mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_set_padding_mode failed");
    }
  }

  instance_data->key_set = true;
  //mrb_incref(&v[0]);
  //mrb_incref(&key);
  return key;
}

static mrb_value
mrb_mbedtls_cipher_iv_len(mrb_state *mrb, mrb_value self)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  return mrb_fixnum_value(instance_data->iv_len);
}

static mrb_value
mrb_mbedtls_cipher_iv_eq(mrb_state *mrb, mrb_value self)
{
  mrb_value iv;
  mrb_get_args(mrb, "S", &iv);

  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  if (instance_data->iv_set) {
    mrb_warn(mrb, "iv should be set once per instance, ignoring\n");
    return iv;
  }
  if (instance_data->operation == MBEDTLS_OPERATION_NONE) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "operation is not set");
  }
  if (RSTRING_LEN(iv) != instance_data->iv_len) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "iv length is invalid");
  }

  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  int ret;
  ret = mbedtls_cipher_set_iv(ctx, (const unsigned char *)RSTRING_PTR(iv), RSTRING_LEN(iv));
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_set_iv failed");
  }
  ret = mbedtls_cipher_reset(ctx);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_reset failed");
  }

  instance_data->iv_set = true;
  //mrb_incref(&v[0]);
  //mrb_incref(&iv);
  return iv;
}

static mrb_value
mrb_mbedtls_cipher_update_ad(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  int ret;
  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  ret = mbedtls_cipher_update_ad(ctx, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_update_ad failed");
  }

  //mrb_incref(&v[0]);
  return self;
}

static mrb_value
mrb_mbedtls_cipher_update(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  size_t out_len = RSTRING_LEN(input) + 16;
  unsigned char* output = (unsigned char *)mrb_malloc(mrb, out_len);
  int ret;

  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  ret = mbedtls_cipher_update(ctx, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input), output, &out_len);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_update failed");
  }
  mrb_value ret_value = mrb_str_new(mrb, (const char *)output, (mrb_int)out_len);
  mrb_free(mrb, output);
  //mrb_incref(&v[0]);
  return ret_value;
}

static mrb_value
mrb_mbedtls_cipher_finish(mrb_state *mrb, mrb_value self)
{
  size_t out_len = 16;
  unsigned char output[out_len];
  int ret;

  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  ret = mbedtls_cipher_finish(ctx, output, &out_len);
  if (ret != 0) {
    char error_buf[200];
    mbedtls_strerror(ret, error_buf, 100);
    snprintf(error_buf + strlen(error_buf), 100, " (error code: %d)", ret);
    mrb_raise(mrb, E_RUNTIME_ERROR, error_buf);
  }
  mrb_value ret_value = mrb_str_new(mrb, (const char *)output, (mrb_int)out_len);
  //mrb_incref(&v[0]);
  return ret_value;
}

static mrb_value
mrb_mbedtls_cipher_write_tag(mrb_state *mrb, mrb_value self)
{
  int ret;
  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  // Get the appropriate tag length for the cipher
  size_t tag_len = 16; // Default for most AEAD modes
  mbedtls_cipher_type_t cipher_type = mbedtls_cipher_get_type(ctx);
  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(cipher_type);
  if (cipher_info != NULL) {
    mbedtls_cipher_mode_t mode = mbedtls_cipher_info_get_mode(cipher_info);
    if (mode == MBEDTLS_MODE_CCM) {
      tag_len = 16; // CCM typically uses 16-byte tags
    } else if (mode == MBEDTLS_MODE_GCM) {
      tag_len = 16; // GCM uses 16-byte tags
    } else if (mode == MBEDTLS_MODE_CHACHAPOLY) {
      tag_len = 16; // ChaCha20-Poly1305 uses 16-byte tags
    }
  }

  // Check if this is an AEAD cipher that supports tags
  if (cipher_info != NULL) {
    mbedtls_cipher_mode_t mode = mbedtls_cipher_info_get_mode(cipher_info);
    const char *cipher_name = mbedtls_cipher_info_get_name(cipher_info);
#ifdef PICORUBY_DEBUG
    printf("Cipher: %s, Mode: %d (CBC=2, GCM=%d, CCM=%d, CHACHAPOLY=%d)\n", 
           cipher_name ? cipher_name : "unknown",
           mode, MBEDTLS_MODE_GCM, MBEDTLS_MODE_CCM, MBEDTLS_MODE_CHACHAPOLY);
#endif
    if (mode != MBEDTLS_MODE_GCM && mode != MBEDTLS_MODE_CCM && mode != MBEDTLS_MODE_CHACHAPOLY) {
      // For non-AEAD modes, return empty string as no tag is available
      return mrb_str_new_cstr(mrb, "");
    }
  }

  unsigned char *tag = mrb_malloc(mrb, tag_len);
  ret = mbedtls_cipher_write_tag(ctx, tag, tag_len);
  if (ret != 0) {
    mrb_free(mrb, tag);
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
#ifdef PICORUBY_DEBUG
    printf("mbedtls_cipher_write_tag failed: %s (ret=%d, 0x%x)\n", error_buf, ret, -ret);
    printf("cipher_type: %d, tag_len: %zu\n", cipher_type, tag_len);
#endif
    mrb_raisef(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_write_tag failed: %s", error_buf);
  }

  mrb_value ret_value = mrb_str_new(mrb, (const char *)tag, (mrb_int)tag_len);
  mrb_free(mrb, tag);
  return ret_value;
}

static mrb_value
mrb_mbedtls_cipher_check_tag(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  cipher_instance_t *instance_data = (cipher_instance_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  // Check if this is an AEAD cipher that supports tags
  mbedtls_cipher_type_t cipher_type = mbedtls_cipher_get_type(ctx);
  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(cipher_type);
  if (cipher_info != NULL) {
    mbedtls_cipher_mode_t mode = mbedtls_cipher_info_get_mode(cipher_info);
    if (mode != MBEDTLS_MODE_GCM && mode != MBEDTLS_MODE_CCM && mode != MBEDTLS_MODE_CHACHAPOLY) {
      // For non-AEAD modes, tag verification is not applicable, assume success
      return mrb_true_value();
    }
  }

  int ret = mbedtls_cipher_check_tag(ctx, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret == MBEDTLS_ERR_CIPHER_AUTH_FAILED) {
    //mrb_incref(&v[0]);
    return mrb_false_value();
  } else if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_check_tag failed");
  }
  //mrb_incref(&v[0]);
  return mrb_true_value();
}

void
gem_mbedtls_cipher_init(mrb_state *mrb, struct RClass *module_MbedTLS)
{
  struct RClass *class_MbedTLS_Cipher = mrb_define_class_under_id(mrb, module_MbedTLS, MRB_SYM(Cipher), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_MbedTLS_Cipher, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(ciphers), mrb_mbedtls_cipher_ciphers, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(initialize), mrb_mbedtls_cipher_initialize, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(encrypt),    mrb_mbedtls_cipher_encrypt, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(decrypt),    mrb_mbedtls_cipher_decrypt, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(key_len),    mrb_mbedtls_cipher_key_len, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM_E(key),      mrb_mbedtls_cipher_key_eq, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(iv_len),     mrb_mbedtls_cipher_iv_len, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM_E(iv),       mrb_mbedtls_cipher_iv_eq, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(update),     mrb_mbedtls_cipher_update, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(update_ad),  mrb_mbedtls_cipher_update_ad, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(finish),     mrb_mbedtls_cipher_finish, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(write_tag),  mrb_mbedtls_cipher_write_tag, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_Cipher, MRB_SYM(check_tag),  mrb_mbedtls_cipher_check_tag, MRB_ARGS_REQ(1));
}

