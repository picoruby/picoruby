#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/data.h"
#include "mruby/class.h"
#include "mruby/string.h"
#include "mbedtls/cmac.h"

static void
mrb_cipher_context_free(mrb_state *mrb, void *ptr)
{
  mbedtls_cipher_free(ptr);
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_cipher_context_type = {
  "CipherContext", mrb_cipher_context_free,
};

static mrb_value
mrb__init_aes(mrb_state *mrb, mrb_value klass)
{
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)mrb_malloc(mrb, sizeof(mbedtls_cipher_context_t));
  mrb_value self = mrb_obj_new(mrb, mrb_class_ptr(klass), 0, NULL);
  DATA_PTR(self) = ctx;
  DATA_TYPE(self) = &mrb_cipher_context_type;
  mbedtls_cipher_init(ctx);
  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
  int ret;
  ret = mbedtls_cipher_setup(ctx, cipher_info);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_setup failed");
  }
  mrb_value key;
  mrb_get_args(mrb, "S", &key);
  ret = mbedtls_cipher_cmac_starts(ctx, (const unsigned char *)RSTRING_PTR(key), RSTRING_LEN(key) * 8); /* last arg is keybits */
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_cmac_starts failed");
  }
  return self;
}

static mrb_value
mrb_update(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_context_type);
  int ret;
  ret = mbedtls_cipher_cmac_update(ctx, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_cmac_update failed");
  }
  return self;
}

static mrb_value
mrb_reset(mrb_state *mrb, mrb_value self)
{
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_context_type);
  int ret = mbedtls_cipher_cmac_reset(ctx);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_cmac_reset failed");
  }
  return self;
}

static mrb_value
mrb_digest(mrb_state *mrb, mrb_value self)
{
  mbedtls_cipher_context_t *ctx = (mbedtls_cipher_context_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_context_type);
  unsigned char output[16];
  int ret;
  ret = mbedtls_cipher_cmac_finish(ctx, output);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_cipher_cmac_finish failed");
  }
  mrb_value digest = mrb_str_new(mrb, (const char *)output, sizeof(output));
  //mrb_incref(&v[0]);
  return digest;
}

void
gem_mbedtls_cmac_init(mrb_state *mrb, struct RClass *module_MbedTLS)
{
  struct RClass *class_MbedTLS_CMAC = mrb_define_class_under_id(mrb, module_MbedTLS, MRB_SYM(CMAC), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_MbedTLS_CMAC, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_MbedTLS_CMAC, MRB_SYM(_init_aes),  mrb__init_aes, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_CMAC, MRB_SYM(update),     mrb_update, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_CMAC, MRB_SYM(reset),      mrb_reset, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_CMAC, MRB_SYM(digest),     mrb_digest, MRB_ARGS_NONE());
}

