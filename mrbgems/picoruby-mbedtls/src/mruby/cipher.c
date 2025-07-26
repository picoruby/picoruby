#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/data.h"
#include "mruby/class.h"
#include "cipher.h"

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
  MbedTLS_cipher_type_key_iv_len(cipher_name, &cipher_type, &key_len, &iv_len);
  if (cipher_type == MBEDTLS_CIPHER_NONE) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "unsupported cipher suite");
  }

  unsigned char * instance_data = mrb_malloc(mrb, MbedTLS_cipher_instance_size());
  DATA_PTR(self) = instance_data;
  DATA_TYPE(self) = &mrb_cipher_type;
  int ret = MbedTLS_cipher_new(instance_data, cipher_type, MBEDTLS_OPERATION_NONE, key_len, iv_len);
  if (ret == CIPHER_NEW_BAD_INPUT_DATA) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setup failed (bad input data)");
  } else if (ret == CIPHER_NEW_FAILED_TO_SETUP) {
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
  MbedTLS_cipher_encrypt(mrb_data_get_ptr(mrb, self, &mrb_cipher_type));
  return self;
}

static mrb_value
mrb_mbedtls_cipher_decrypt(mrb_state *mrb, mrb_value self)
{
  MbedTLS_cipher_decrypt(mrb_data_get_ptr(mrb, self, &mrb_cipher_type));
  return self;
}

static mrb_value
mrb_mbedtls_cipher_key_len(mrb_state *mrb, mrb_value self)
{
  uint8_t key_len = MbedTLS_cipher_key_len(mrb_data_get_ptr(mrb, self, &mrb_cipher_type));
  return mrb_fixnum_value(key_len);
}

static mrb_value
mrb_mbedtls_cipher_key_eq(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_get_args(mrb, "S", &key);

  int ret = MbedTLS_cipher_key_eq(mrb_data_get_ptr(mrb, self, &mrb_cipher_type), RSTRING_PTR(key), RSTRING_LEN(key));
  if (ret == CIPHER_KEY_EQ_DOUBLE) {
    mrb_warn(mrb, "key should be set once per instance, ignoring\n");
    return key;
  } else if (ret == CIPHER_KEY_EQ_OPERATION_NOT_SET) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "operation is not set");
  } else if (ret == CIPHER_KEY_EQ_INVALID_KEY_LENGTH) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "key length is invalid");
  } else if (ret == CIPHER_KEY_EQ_FAILED_TO_SET_KEY) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setkey failed");
  } else if (ret == CIPHER_KEY_EQ_FAILED_TO_SET_PADDING_MODE) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_set_padding_mode failed");
  }

  //mrb_incref(&v[0]);
  //mrb_incref(&key);
  return key;
}

static mrb_value
mrb_mbedtls_cipher_iv_len(mrb_state *mrb, mrb_value self)
{
  uint8_t iv_len = MbedTLS_cipher_iv_len(mrb_data_get_ptr(mrb, self, &mrb_cipher_type));
  return mrb_fixnum_value(iv_len);
}

static mrb_value
mrb_mbedtls_cipher_iv_eq(mrb_state *mrb, mrb_value self)
{
  mrb_value iv;
  mrb_get_args(mrb, "S", &iv);

  int ret = MbedTLS_cipher_iv_eq(mrb_data_get_ptr(mrb, self, &mrb_cipher_type), RSTRING_PTR(iv), RSTRING_LEN(iv));
  if (ret == CIPHER_IV_EQ_DOUBLE) {
    mrb_warn(mrb, "iv should be set once per instance, ignoring\n");
    return iv;
  } else if (ret == CIPHER_IV_EQ_OPERATION_NOT_SET) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "operation is not set");
  } else if (ret == CIPHER_IV_EQ_INVALID_IV_LENGTH) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "iv length is invalid");
  } else if (ret == CIPHER_IV_EQ_FAILED_TO_SET_IV) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_set_iv failed");
  } else if (ret == CIPHER_IV_EQ_FAILED_TO_RESET) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_reset failed");
  }

  //mrb_incref(&v[0]);
  //mrb_incref(&iv);
  return iv;
}

static mrb_value
mrb_mbedtls_cipher_update_ad(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  int ret = MbedTLS_cipher_update_ad(mrb_data_get_ptr(mrb, self, &mrb_cipher_type), (const char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret == CIPHER_UPDATE_AD_FAILED) {
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

  int ret = MbedTLS_cipher_update(mrb_data_get_ptr(mrb, self, &mrb_cipher_type), (const char *)RSTRING_PTR(input), RSTRING_LEN(input), output, &out_len);
  if (ret == CIPHER_UPDATE_FAILED) {
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
  int ret = MbedTLS_cipher_finish(mrb_data_get_ptr(mrb, self, &mrb_cipher_type), output, &out_len);
  if (ret == CIPHER_FINISH_FAILED) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_finish failed");
  }
  mrb_value ret_value = mrb_str_new(mrb, (const char *)output, (mrb_int)out_len);
  //mrb_incref(&v[0]);
  return ret_value;
}

static mrb_value
mrb_mbedtls_cipher_write_tag(mrb_state *mrb, mrb_value self)
{
  size_t tag_len = 16;
  unsigned char tag[tag_len];

  int ret = MbedTLS_cipher_write_tag(mrb_data_get_ptr(mrb, self, &mrb_cipher_type), (const char *)tag, tag_len);
  if (ret == CIPHER_WRITE_TAG_FAILED) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_write_tag failed");
  }

  mrb_value ret_value = mrb_str_new(mrb, (const char *)tag, (mrb_int)tag_len);
  //mrb_incref(&v[0]);
  return ret_value;
}

static mrb_value
mrb_mbedtls_cipher_check_tag(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  int ret = MbedTLS_cipher_check_tag(mrb_data_get_ptr(mrb, self, &mrb_cipher_type), (const char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret == CIPHER_CHECK_TAG_AUTH_FAILED) {
    // mrb_incref(&self);
    return mrb_false_value();
  } else if (ret == CIPHER_CHECK_TAG_FAILED) {
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

