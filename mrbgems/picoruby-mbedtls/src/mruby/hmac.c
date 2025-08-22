#include <stdio.h>
#include <string.h>
#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/data.h"
#include "mruby/class.h"
#include "mbedtls/md.h"

#include "md_context.h"

static mrb_value
mrb_initialize(mrb_state *mrb, mrb_value self)
{
  const char *algorithm;
  mrb_value key;
  mrb_get_args(mrb, "zS", &algorithm, &key);

  if (strcmp(algorithm, "sha256") != 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "unsupported hash algorithm");
  }

  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)mrb_malloc(mrb, sizeof(mbedtls_md_context_t));
  DATA_PTR(self) = ctx;
  DATA_TYPE(self) = &mrb_md_context_type;
  mbedtls_md_init(ctx);
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  int ret;
  ret = mbedtls_md_setup(ctx, md_info, 1); // 1 for HMAC
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_setup failed");
  }

  ret = mbedtls_md_hmac_starts(ctx, (const unsigned char *)RSTRING_PTR(key), RSTRING_LEN(key));
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_hmac_starts failed");
  }
  return self;
}

static int
check_finished(mrb_state *mrb, mbedtls_md_context_t *ctx)
{
  if (mbedtls_md_info_from_ctx(ctx) == NULL) { // mbedtls_md_free() makes md_info NULL
    return -1;
  }
  return 0;
}

static mrb_value
mrb_update(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)mrb_data_get_ptr(mrb, self, &mrb_md_context_type);
  if (check_finished(mrb, ctx) != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "already finished");
  }
  int ret;
  ret = mbedtls_md_hmac_update(ctx, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_hmac_update failed");
  }
  return self;
}

static mrb_value
mrb_reset(mrb_state *mrb, mrb_value self)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)mrb_data_get_ptr(mrb, self, &mrb_md_context_type);
  int ret = mbedtls_md_hmac_reset(ctx);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_hmac_reset failed");
  }
  return self;
}

static int
finish(mrb_state *mrb, unsigned char *output, mbedtls_md_context_t *ctx)
{
  int ret;
  if (check_finished(mrb, ctx) != 0) {
    return -1;
  }
  ret = mbedtls_md_hmac_finish(ctx, output);
  if (ret != 0) {
    return -1;
  }
  return 0;
}

static mrb_value
mrb_digest(mrb_state *mrb, mrb_value self)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)mrb_data_get_ptr(mrb, self, &mrb_md_context_type);
  unsigned char output[32]; // SHA256 produces 32 bytes output
  if (finish(mrb, output, ctx) != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_hmac_finish failed");
  }
  mrb_value digest = mrb_str_new(mrb, (const char *)output, sizeof(output));
  //mrb_incref(&v[0]);
  return digest;
}

static mrb_value
mrb_hexdigest(mrb_state *mrb, mrb_value self)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)mrb_data_get_ptr(mrb, self, &mrb_md_context_type);
  unsigned char output[32]; // SHA256 produces 32 bytes output
  if (finish(mrb, output, ctx) != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_hmac_finish failed");
  }
  char hexdigest[64]; // 32 bytes * 2
  for (int i = 0; i < 32; i++) {
    sprintf(hexdigest + i * 2, "%02x", output[i]);
  }
  mrb_value result = mrb_str_new_cstr(mrb, hexdigest);
  //mrb_incref(&v[0]);
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

