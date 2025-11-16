#include <stdbool.h>
#include <string.h>

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/variable.h"
#include "mruby/error.h"

#include "pkey.h"
#include "md_context.h" // for mrb_md_context_type

static void
mrb_pkey_free(mrb_state *mrb, void *ptr)
{
  if (ptr) {
    MbedTLS_pkey_free(ptr);
    mrb_free(mrb, ptr);
  }
}

static const struct mrb_data_type mrb_pkey_type = {
  "PKey", mrb_pkey_free,
};

static struct RClass *class_MbedTLS_PKey_PKeyError;

static void
raise_pkey_error(mrb_state *mrb, int ret)
{
  char error_buf[100];
  MbedTLS_pkey_strerror(ret, error_buf, sizeof(error_buf));
  mrb_raisef(mrb, class_MbedTLS_PKey_PKeyError, "PKey error: %s (ret=%d)", error_buf, ret);
}

static mrb_value
mrb_mbedtls_pkey_rsa_public_p(mrb_state *mrb, mrb_value self)
{
  void *pk = mrb_data_get_ptr(mrb, self, &mrb_pkey_type);
  return mrb_bool_value(MbedTLS_pkey_is_public(pk));
}

static mrb_value
mrb_mbedtls_pkey_rsa_private_p(mrb_state *mrb, mrb_value self)
{
  void *pk = mrb_data_get_ptr(mrb, self, &mrb_pkey_type);
  return mrb_bool_value(MbedTLS_pkey_is_private(pk));
}

static mrb_value
mrb_mbedtls_pkey_rsa_free(mrb_state *mrb, mrb_value self)
{
  // For mruby, GC handles freeing. This is for mruby/c compatibility.
  return mrb_nil_value();
}

static mrb_value
mrb_mbedtls_pkey_rsa_s_generate(mrb_state *mrb, mrb_value klass)
{
  mrb_int bits;
  mrb_int exponent = 65537;
  mrb_get_args(mrb, "i|i", &bits, &exponent);

  mrb_value self = mrb_obj_new(mrb, mrb_class_ptr(klass), 0, NULL);
  void *pk = mrb_malloc(mrb, MbedTLS_pkey_instance_size());
  DATA_PTR(self) = pk;
  DATA_TYPE(self) = &mrb_pkey_type;
  MbedTLS_pkey_init(pk);

  mrb_value mbedtls_pers = mrb_gv_get(mrb, MRB_GVSYM(_mbedtls_pers));
  int ret = MbedTLS_pkey_generate_rsa(pk, bits, exponent, (const unsigned char *)RSTRING_PTR(mbedtls_pers), RSTRING_LEN(mbedtls_pers));

  if (ret != 0) {
    raise_pkey_error(mrb, ret);
  }

  return self;
}

static mrb_value
mrb_mbedtls_pkey_rsa_public_key(mrb_state *mrb, mrb_value self) {
  void *orig_pk = mrb_data_get_ptr(mrb, self, &mrb_pkey_type);

  mrb_value new_obj = mrb_obj_new(mrb, mrb_obj_class(mrb, self), 0, NULL);
  void *new_pk = mrb_malloc(mrb, MbedTLS_pkey_instance_size());
  DATA_PTR(new_obj) = new_pk;
  DATA_TYPE(new_obj) = &mrb_pkey_type;
  MbedTLS_pkey_init(new_pk);

  int ret = MbedTLS_pkey_get_public_key(new_pk, orig_pk);
  if (ret != 0) {
    raise_pkey_error(mrb, ret);
  }

  return new_obj;
}

static mrb_value
mrb_mbedtls_pkey_rsa_to_pem(mrb_state *mrb, mrb_value self)
{
  void *pk = mrb_data_get_ptr(mrb, self, &mrb_pkey_type);
  unsigned char buf[4096];

  int ret = MbedTLS_pkey_to_pem(pk, buf, sizeof(buf));
  if (ret != 0) {
    raise_pkey_error(mrb, ret);
  }

  return mrb_str_new_cstr(mrb, (char *)buf);
}

static mrb_value
mrb_mbedtls_pkey_rsa_s_new(mrb_state *mrb, mrb_value klass)
{
  mrb_value arg1;
  mrb_get_args(mrb, "o", &arg1);

  if (mrb_fixnum_p(arg1)) {
    // For backward compatibility: RSA.new(2048) should work like RSA.generate(2048)
    return mrb_funcall(mrb, klass, "generate", 1, arg1);
  }
  if (!mrb_string_p(arg1)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "argument must be String or Integer");
  }

  // Ensure null termination for mbedtls
  mrb_value pem_str = mrb_str_dup(mrb, arg1);
  mrb_str_cat_cstr(mrb, pem_str, "");

  mrb_value self = mrb_obj_new(mrb, mrb_class_ptr(klass), 0, NULL);
  void *pk = mrb_malloc(mrb, MbedTLS_pkey_instance_size());
  DATA_PTR(self) = pk;
  DATA_TYPE(self) = &mrb_pkey_type;
  MbedTLS_pkey_init(pk);

  int ret = MbedTLS_pkey_from_pem(pk, (const unsigned char *)RSTRING_PTR(pem_str), RSTRING_LEN(pem_str) + 1);
  if (ret != 0) {
    if (ret == PKEY_ERR_KEY_TYPE_NOT_RSA) {
      mrb_raise(mrb, class_MbedTLS_PKey_PKeyError, "Key type is not RSA");
    } else {
      raise_pkey_error(mrb, ret);
    }
  }

  return self;
}

static mrb_value
mrb_mbedtls_pkey_pkeybase_verify(mrb_state *mrb, mrb_value self)
{
  mrb_value digest_obj, sig_str, input_str;
  mrb_get_args(mrb, "oSS", &digest_obj, &sig_str, &input_str);

  void *pk = mrb_data_get_ptr(mrb, self, &mrb_pkey_type);

  const unsigned char *md_ctx = mrb_data_get_ptr(mrb, digest_obj, &mrb_md_context_type);

  int ret = MbedTLS_pkey_verify(pk, md_ctx, (const unsigned char *)RSTRING_PTR(input_str), RSTRING_LEN(input_str), (const unsigned char *)RSTRING_PTR(sig_str), RSTRING_LEN(sig_str));
  if (ret == PKEY_CREATE_MD_FAILED) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Hash calculation failed");
  }

  return mrb_bool_value(ret == PKEY_SUCCESS);
}

static mrb_value
mrb_mbedtls_pkey_pkeybase_sign(mrb_state *mrb, mrb_value self)
{
  mrb_value digest_obj, input_str;
  mrb_get_args(mrb, "oS", &digest_obj, &input_str);

  void *pk = mrb_data_get_ptr(mrb, self, &mrb_pkey_type);

  const unsigned char *md_ctx = mrb_data_get_ptr(mrb, digest_obj, &mrb_md_context_type);

  unsigned char sig[PKEY_SIGNATURE_MAX_SIZE];
  size_t sig_len;
  mrb_value mbedtls_pers = mrb_gv_get(mrb, MRB_GVSYM(_mbedtls_pers));

  int ret = MbedTLS_pkey_sign(pk, md_ctx, (const unsigned char *)RSTRING_PTR(input_str), RSTRING_LEN(input_str), sig, sizeof(sig), &sig_len, (const unsigned char *)RSTRING_PTR(mbedtls_pers), RSTRING_LEN(mbedtls_pers));
  if (ret == PKEY_CREATE_MD_FAILED) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to calculate hash");
  } else if (ret != 0) {
    raise_pkey_error(mrb, ret);
  }

  return mrb_str_new(mrb, (const char *)sig, sig_len);
}


void
gem_mbedtls_pkey_init(mrb_state *mrb, struct RClass *module_MbedTLS)
{
  struct RClass *module_MbedTLS_PKey = mrb_define_module_under_id(mrb, module_MbedTLS, MRB_SYM(PKey));
  struct RClass *class_MbedTLS_PKey_PKeyBase = mrb_define_class_under_id(mrb, module_MbedTLS_PKey, MRB_SYM(PKeyBase), mrb->object_class);
  mrb_define_method_id(mrb, class_MbedTLS_PKey_PKeyBase, MRB_SYM(sign),   mrb_mbedtls_pkey_pkeybase_sign, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_MbedTLS_PKey_PKeyBase, MRB_SYM(verify), mrb_mbedtls_pkey_pkeybase_verify, MRB_ARGS_REQ(3));

  struct RClass *class_MbedTLS_PKey_RSA = mrb_define_class_under_id(mrb, module_MbedTLS_PKey, MRB_SYM(RSA), class_MbedTLS_PKey_PKeyBase);
  MRB_SET_INSTANCE_TT(class_MbedTLS_PKey_RSA, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(new),      mrb_mbedtls_pkey_rsa_s_new, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(generate), mrb_mbedtls_pkey_rsa_s_generate, MRB_ARGS_ARG(1,1));
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(public_key),  mrb_mbedtls_pkey_rsa_public_key, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(export),      mrb_mbedtls_pkey_rsa_to_pem, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(to_pem),      mrb_mbedtls_pkey_rsa_to_pem, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(to_s),        mrb_mbedtls_pkey_rsa_to_pem, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM_Q(public),    mrb_mbedtls_pkey_rsa_public_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM_Q(private),   mrb_mbedtls_pkey_rsa_private_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(free),        mrb_mbedtls_pkey_rsa_free, MRB_ARGS_NONE());

  class_MbedTLS_PKey_PKeyError = mrb_define_class_under_id(mrb, module_MbedTLS_PKey, MRB_SYM(PKeyError), E_STANDARD_ERROR);
}
