#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"

#include "md_context.h"

static mrb_value
mrb_mbedtls_digest_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value algorithm;
  mrb_get_args(mrb, "n", &algorithm);

  int alg = mbedtls_digest_algorithm_name((const char *)mrb_sym_name(mrb, mrb_symbol(algorithm)));
  if (alg == -1) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid algorithm");
  }

  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)mrb_malloc(mrb, sizeof(mbedtls_md_context_t));
  DATA_PTR(self) = ctx;
  DATA_TYPE(self) = &mrb_md_context_type;
  mbedtls_md_init(ctx); const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type((mbedtls_md_type_t)alg);
  int ret;
  ret = mbedtls_md_setup(ctx, md_info, 0);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_setup failed");
  }
  ret = mbedtls_md_starts(ctx);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_starts failed");
  }
  return self;
}

static mrb_value
mrb_mbedtls_digest_free(mrb_state *mrb, mrb_value self)
{
  // no-op for compatibility with mruby/c
  return mrb_nil_value();
}

static mrb_value
mrb_mbedtls_digest_update(mrb_state *mrb, mrb_value self)
{
  mrb_value input;
  mrb_get_args(mrb, "S", &input);

  int ret;
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)mrb_data_get_ptr(mrb, self, &mrb_md_context_type);
  ret = mbedtls_md_update(ctx, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input));
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_update failed");
  }
  //mrb_incref(&v[0]);
  return self;
}

static mrb_value
mrb_mbedtls_digest_finish(mrb_state *mrb, mrb_value self)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)mrb_data_get_ptr(mrb, self, &mrb_md_context_type);

  size_t out_len = mbedtls_md_get_size(ctx->md_info);
  unsigned char* output = mrb_malloc(mrb, out_len); // need at least block size
  int ret;

  ret = mbedtls_md_finish(ctx, output);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_digest_finish failed");
  }
  mrb_value ret_value = mrb_str_new(mrb, (const char *)output, out_len);
  mrb_free(mrb, output);
  //mrb_incref(&v[0]);
  return ret_value;
}

void
gem_mbedtls_digest_init(mrb_state *mrb, struct RClass *module_MbedTLS)
{
  struct RClass *class_MbedTLS_Digest = mrb_define_class_under_id(mrb, module_MbedTLS, MRB_SYM(Digest), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_MbedTLS_Digest, MRB_TT_CDATA);

  mrb_define_method_id(mrb, class_MbedTLS_Digest, MRB_SYM(initialize),  mrb_mbedtls_digest_initialize, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_Digest, MRB_SYM(update),      mrb_mbedtls_digest_update, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_Digest, MRB_SYM(finish),      mrb_mbedtls_digest_finish, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_Digest, MRB_SYM(free),        mrb_mbedtls_digest_free, MRB_ARGS_NONE());
}

