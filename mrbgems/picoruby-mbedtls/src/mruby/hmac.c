#include <stdio.h>
#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/data.h"
#include "mruby/class.h"
#include "hmac.h"

static void
mrb_hmac_context_free(mrb_state *mrb, void *ptr)
{
  MbedTLS_hmac_free(ptr);
  mrb_free(mrb, ptr);
}

static const struct mrb_data_type mrb_hmac_context_type = {
  "HmacContext", mrb_hmac_context_free,
};

static mrb_value
mrb_initialize(mrb_state *mrb, mrb_value self)
{
  const char *algorithm;
  mrb_value key;
  mrb_get_args(mrb, "Sz", &key, &algorithm);

  void *hmac_instance = mrb_malloc(mrb, MbedTLS_hmac_instance_size());
  DATA_PTR(self) = hmac_instance;
  DATA_TYPE(self) = &mrb_hmac_context_type;

  int ret = MbedTLS_hmac_init(hmac_instance, algorithm, (const unsigned char *)RSTRING_PTR(key), RSTRING_LEN(key));
  if (ret != HMAC_SUCCESS) {
    // TODO: more specific error message
    mrb_raise(mrb, E_ARGUMENT_ERROR, "HMAC init failed");
  }

  return self;
}

static mrb_value
mrb_update(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  void *hmac_instance = mrb_data_get_ptr(mrb, self, &mrb_hmac_context_type);
  int ret = MbedTLS_hmac_update(hmac_instance, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret != HMAC_SUCCESS) {
    if (ret == HMAC_ALREADY_FINISHED) {
      mrb_raise(mrb, E_RUNTIME_ERROR, "already finished");
    } else {
      mrb_raise(mrb, E_RUNTIME_ERROR, "HMAC update failed");
    }
  }
  return self;
}

static mrb_value
mrb_reset(mrb_state *mrb, mrb_value self)
{
  void *hmac_instance = mrb_data_get_ptr(mrb, self, &mrb_hmac_context_type);
  int ret = MbedTLS_hmac_reset(hmac_instance);
  if (ret != HMAC_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "HMAC reset failed");
  }
  return self;
}

static mrb_value
mrb_digest(mrb_state *mrb, mrb_value self)
{
  void *hmac_instance = mrb_data_get_ptr(mrb, self, &mrb_hmac_context_type);
  unsigned char output[32]; // SHA256 produces 32 bytes output
  int ret = MbedTLS_hmac_finish(hmac_instance, output);
  if (ret != HMAC_SUCCESS) {
    if (ret == HMAC_ALREADY_FINISHED) {
      mrb_raise(mrb, E_RUNTIME_ERROR, "already finished");
    } else {
      mrb_raise(mrb, E_RUNTIME_ERROR, "HMAC finish failed");
    }
  }
  mrb_value digest = mrb_str_new(mrb, (const char *)output, sizeof(output));
  return digest;
}

static mrb_value
mrb_hexdigest(mrb_state *mrb, mrb_value self)
{
  void *hmac_instance = mrb_data_get_ptr(mrb, self, &mrb_hmac_context_type);
  unsigned char output[32]; // SHA256 produces 32 bytes output
  if (MbedTLS_hmac_finish(hmac_instance, output) != HMAC_SUCCESS) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "HMAC finish failed");
  }
  char hexdigest[65]; // 32 bytes * 2 + 1
  for (int i = 0; i < 32; i++) {
    sprintf(hexdigest + i * 2, "%02x", output[i]);
  }
  hexdigest[64] = '\0';
  mrb_value result = mrb_str_new_cstr(mrb, hexdigest);
  return result;
}

void
gem_mbedtls_hmac_init(mrb_state *mrb, struct RClass *module_MbedTLS)
{
  struct RClass *class_MbedTLS_HMAC = mrb_define_class_under_id(mrb, module_MbedTLS, MRB_SYM(HMAC), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_MbedTLS_HMAC, MRB_TT_CDATA);

  mrb_define_method_id(mrb, class_MbedTLS_HMAC, MRB_SYM(initialize),  mrb_initialize, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_MbedTLS_HMAC, MRB_SYM(update),      mrb_update,     MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_HMAC, MRB_SYM(reset),       mrb_reset,      MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_HMAC, MRB_SYM(digest),      mrb_digest,     MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_HMAC, MRB_SYM(hexdigest),   mrb_hexdigest,  MRB_ARGS_NONE());
}

