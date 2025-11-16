#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/data.h"
#include "mruby/class.h"

#include "digest.h"
#include "md.h"
#include "md_context.h"

struct mrb_data_type mrb_md_context_type = {
  "MdContext", mrb_md_context_free,
};

void
mrb_md_context_free(mrb_state *mrb, void *ptr)
{
  MbedTLS_md_free(ptr);
  mrb_free(mrb, ptr);
}

static mrb_value
mrb_mbedtls_digest_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_sym algorithm;
  mrb_get_args(mrb, "n", &algorithm);

  int alg = MbedTLS_digest_algorithm_name((const char *)mrb_sym_name(mrb, algorithm));
  if (alg == -1) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "invalid algorithm");
  }

  unsigned char *ctx = (unsigned char *)mrb_malloc(mrb, MbedTLS_digest_instance_size());
  DATA_PTR(self) = ctx;
  DATA_TYPE(self) = &mrb_md_context_type;

  int ret = MbedTLS_digest_new(ctx, alg);
  if (ret == DIGEST_FAILED_TO_SETUP) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_setup failed");
  } else if (ret == DIGEST_FAILED_TO_START) {
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

  unsigned char *ctx = (unsigned char *)mrb_data_get_ptr(mrb, self, &mrb_md_context_type);
  if (MbedTLS_digest_update(ctx, (const unsigned char *)RSTRING_PTR(input), RSTRING_LEN(input)) != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_md_update failed");
  }
  return self;
}

static mrb_value
mrb_mbedtls_digest_finish(mrb_state *mrb, mrb_value self)
{
  unsigned char *ctx = (unsigned char *)mrb_data_get_ptr(mrb, self, &mrb_md_context_type);

  size_t out_len = MbedTLS_digest_get_size(ctx);
  unsigned char* output = mrb_malloc(mrb, out_len);

  if (MbedTLS_digest_finish(ctx, output) != 0) {
    mrb_free(mrb, output);
    mrb_raise(mrb, E_RUNTIME_ERROR, "mbedtls_digest_finish failed");
  }
  mrb_value ret_value = mrb_str_new(mrb, (const char *)output, (mrb_int)out_len);
  mrb_free(mrb, output);
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
