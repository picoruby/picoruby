#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/data.h"
#include "mruby/class.h"
#include "mruby/string.h"
#include "cmac.h"

static void
mrb_cipher_context_free(mrb_state *mrb, void *ptr)
{
  MbedTLS_cmac_free(ptr);
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_cipher_context_type = {
  "CipherContext", mrb_cipher_context_free,
};

static mrb_value
mrb__init_aes(mrb_state *mrb, mrb_value klass)
{
  uint8_t *cmac_instance = mrb_malloc(mrb, MbedTLS_cmac_instance_size());
  mrb_value self = mrb_obj_new(mrb, mrb_class_ptr(klass), 0, NULL);
  DATA_PTR(self) = cmac_instance;
  DATA_TYPE(self) = &mrb_cipher_context_type;
  mrb_value key;
  mrb_get_args(mrb, "S", &key);
  int ret = MbedTLS_cmac_init_aes(cmac_instance, (const unsigned char *)RSTRING_PTR(key), RSTRING_LEN(key) * 8);
  if (ret != CMAC_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "CMAC init failed");
  }
  return self;
}

static mrb_value
mrb_update(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  uint8_t *cmac_instance = (uint8_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_context_type);
  int ret;
  ret = MbedTLS_cmac_update(cmac_instance, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret != CMAC_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "CMAC update failed");
  }
  return self;
}

static mrb_value
mrb_reset(mrb_state *mrb, mrb_value self)
{
  uint8_t *cmac_instance = (uint8_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_context_type);
  int ret = MbedTLS_cmac_reset(cmac_instance);
  if (ret != CMAC_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "CMAC reset failed");
  }
  return self;
}

static mrb_value
mrb_digest(mrb_state *mrb, mrb_value self)
{
  uint8_t *cmac_instance = (uint8_t *)mrb_data_get_ptr(mrb, self, &mrb_cipher_context_type);
  unsigned char output[16];
  int ret;
  ret = MbedTLS_cmac_finish(cmac_instance, output);
  if (ret != CMAC_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "CMAC finish failed");
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

