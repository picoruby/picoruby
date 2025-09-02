#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/data.h"
#include "mruby/class.h"

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

  int cipher_type;
  uint8_t key_len, iv_len;
#ifdef PICORUBY_DEBUG
  printf("Cipher.new: cipher_name='%s'\n", cipher_name);
#endif
  Mbedtls_cipher_type_key_iv_len(cipher_name, &cipher_type, &key_len, &iv_len);
#ifdef PICORUBY_DEBUG
  printf("Cipher.new: cipher_type=%d, key_len=%d, iv_len=%d\n", cipher_type, key_len, iv_len);
#endif
  if (cipher_type == 0) {
#ifdef PICORUBY_DEBUG
    printf("Cipher.new: unsupported cipher suite '%s'\n", cipher_name);
#endif
    mrb_raise(mrb, E_ARGUMENT_ERROR, "unsupported cipher suite");
  }

  uint8_t *cipher_instance = mrb_malloc(mrb, MbedTLS_cipher_instance_size());

  DATA_PTR(self) = cipher_instance;
  DATA_TYPE(self) = &mrb_cipher_type;

  int ret = MbedTLS_cipher_new(cipher_instance, cipher_type, key_len, iv_len);
  if (ret == CIPHER_BAD_INPUT_DATA) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setup failed (bad input data)");
  } else if (ret == CIPHER_FAILED_TO_SETUP) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setup failed");
  }
  return self;
}

static mrb_value
mrb_mbedtls_cipher_ciphers(mrb_state *mrb, mrb_value self)
{
  mrb_value ret = mrb_ary_new_capa(mrb, CIPHER_SUITES_COUNT);
  for (int i = 0; i < CIPHER_SUITES_COUNT; i++) {
    const char *cipher_name = MbedTLS_cipher_cipher_name(i);
    mrb_value str = mrb_str_new_cstr(mrb, cipher_name);
    mrb_ary_set(mrb, ret, i, str);
  }
  return ret;
}

static mrb_value
mrb_mbedtls_cipher_encrypt(mrb_state *mrb, mrb_value self)
{
  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  MbedTLS_cipher_set_encrypt_to_operation(cipher_instance);
  return self;
}

static mrb_value
mrb_mbedtls_cipher_decrypt(mrb_state *mrb, mrb_value self)
{
  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  MbedTLS_cipher_set_decrypt_to_operation(cipher_instance);
  return self;
}

static mrb_value
mrb_mbedtls_cipher_key_len(mrb_state *mrb, mrb_value self)
{
  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  return mrb_fixnum_value(MbedTLS_cipher_get_key_len((const uint8_t *)cipher_instance));
}

static mrb_value
mrb_mbedtls_cipher_key_eq(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_get_args(mrb, "S", &key);

  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);

  int ret = MbedTLS_cipher_set_key(cipher_instance, (const uint8_t *)RSTRING_PTR(key), RSTRING_LEN(key) * 8);

  if (ret == CIPHER_ALREADY_SET) {
    mrb_warn(mrb, "key should be set once per instance, ignoring\n");
  } else if (ret == CIPHER_OPERATION_NOT_SET) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "operation is not set");
  } else if (ret == CIPHER_INVALID_LENGTH) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "key length is invalid");
  } else if (ret != CIPHER_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setkey failed");
  }

  return key;
}

static mrb_value
mrb_mbedtls_cipher_iv_len(mrb_state *mrb, mrb_value self)
{
  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);
  return mrb_fixnum_value(MbedTLS_cipher_get_iv_len(cipher_instance));
}

static mrb_value
mrb_mbedtls_cipher_iv_eq(mrb_state *mrb, mrb_value self)
{
  mrb_value iv;
  mrb_get_args(mrb, "S", &iv);

  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);

  int ret = MbedTLS_cipher_set_iv(cipher_instance, (const uint8_t *)RSTRING_PTR(iv), RSTRING_LEN(iv));

  if (ret == CIPHER_ALREADY_SET) {
    mrb_warn(mrb, "iv should be set once per instance, ignoring\n");
  } else if (ret == CIPHER_OPERATION_NOT_SET) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "operation is not set");
  } else if (ret == CIPHER_INVALID_LENGTH) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "iv length is invalid");
  } else if (ret != CIPHER_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_set_iv failed");
  }

  return iv;
}

static mrb_value
mrb_mbedtls_cipher_update_ad(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);

  if (MbedTLS_cipher_update_ad(cipher_instance, (const uint8_t *)RSTRING_PTR(input), RSTRING_LEN(input)) != CIPHER_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_update_ad failed");
  }

  return self;
}

static mrb_value
mrb_mbedtls_cipher_update(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  size_t out_len = RSTRING_LEN(input) + 16;
  unsigned char* output = (unsigned char *)mrb_malloc(mrb, out_len);

  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);

  if (MbedTLS_cipher_update(cipher_instance, (const uint8_t *)RSTRING_PTR(input), RSTRING_LEN(input), output, &out_len) != CIPHER_SUCCESS) {
    mrb_free(mrb, output);
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_update failed");
  }

  mrb_value ret_value = mrb_str_new(mrb, (const char *)output, (mrb_int)out_len);
  mrb_free(mrb, output);
  return ret_value;
}

static mrb_value
mrb_mbedtls_cipher_finish(mrb_state *mrb, mrb_value self)
{
  size_t out_len = 16;
  unsigned char output[out_len];
  int mbedtls_err;

  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);

  if (MbedTLS_cipher_finish(cipher_instance, output, &out_len, &mbedtls_err) != CIPHER_SUCCESS) {
    char error_buf[200];
    MbedTLS_strerror(mbedtls_err, error_buf, 100);
    snprintf(error_buf + strlen(error_buf), 100, " (error code: %d)", mbedtls_err);
    mrb_raise(mrb, E_RUNTIME_ERROR, error_buf);
  }
  mrb_value ret_value = mrb_str_new(mrb, (const char *)output, (mrb_int)out_len);
  return ret_value;
}

static mrb_value
mrb_mbedtls_cipher_write_tag(mrb_state *mrb, mrb_value self)
{
  size_t tag_len = 16;
  unsigned char *tag = mrb_malloc(mrb, tag_len);
  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);

  int ret = MbedTLS_cipher_write_tag(cipher_instance, tag, &tag_len);

  if (ret == CIPHER_NOT_AEAD) {
    mrb_free(mrb, tag);
    return mrb_str_new_cstr(mrb, "");
  }
  if (ret != CIPHER_SUCCESS) {
    mrb_free(mrb, tag);
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_write_tag failed");
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

  uint8_t *cipher_instance = mrb_data_get_ptr(mrb, self, &mrb_cipher_type);

  int ret = MbedTLS_cipher_check_tag(cipher_instance, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input));

  if (ret == CIPHER_AUTH_FAILED) {
    return mrb_false_value();
  } else if (ret != CIPHER_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_check_tag failed");
  }

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

