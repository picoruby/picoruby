#include <stdbool.h>
#include <string.h>

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/variable.h"

#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "mbedtls/error.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/bignum.h"

#include "md_context.h"

static void
mrb_pkey_free(mrb_state *mrb, void *ptr)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)ptr;
  if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_RSA) {
    mbedtls_pk_free(pk);
  }
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_pkey_type = {
  "PKey", mrb_pkey_free,
};

static struct RClass *class_MbedTLS_PKey_PKeyError;

static mrb_value
mrb_mbedtls_pkey_rsa_public_p(mrb_state *mrb, mrb_value self)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)mrb_data_get_ptr(mrb, self, &mrb_pkey_type);
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
  if (rsa->N.p != NULL && rsa->E.p != NULL) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mbedtls_pkey_rsa_private_p(mrb_state *mrb, mrb_value self)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)mrb_data_get_ptr(mrb, self, &mrb_pkey_type);
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
  if (rsa->D.p != NULL) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mbedtls_pkey_rsa_free(mrb_state *mrb, mrb_value self)
{
  // no-op. for compatibility with mruby/c
  return mrb_nil_value();
}

static mrb_value
mrb_mbedtls_pkey_rsa_s_generate(mrb_state *mrb, mrb_value klass)
{
  mrb_int bits;
  mrb_int exponent = 65537;
  mrb_get_args(mrb, "i|i", &bits, &exponent);

  mrb_value self = mrb_obj_new(mrb, mrb_class_ptr(klass), 0, NULL);
  mbedtls_pk_context *pk = (mbedtls_pk_context *)mrb_malloc(mrb, sizeof(mbedtls_pk_context));
  DATA_PTR(self) = pk;
  DATA_TYPE(self) = &mrb_pkey_type;
  mbedtls_pk_init(pk);

  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  int ret = 1;

  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mrb_value mbedtls_pers = mrb_gv_get(mrb, MRB_GVSYM(_mbedtls_pers));
  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *)RSTRING_PTR(mbedtls_pers),
                              RSTRING_LEN(mbedtls_pers));
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mrb_raisef(mrb, class_MbedTLS_PKey_PKeyError,
                "Failed to seed RNG. ret=%d (0x%x): %s",
                ret, (unsigned int)-ret, error_buf);
  }

  ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  if (ret != 0) {
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mrb_raisef(mrb, class_MbedTLS_PKey_PKeyError, "Failed to setup RSA context");
  }

  ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(*pk),
                           mbedtls_ctr_drbg_random,
                           &ctr_drbg,
                           bits,
                           exponent);
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mrb_raisef(mrb, class_MbedTLS_PKey_PKeyError,
                "Failed to generate RSA key. ret=%d (0x%x): %s",
                ret, (unsigned int)-ret, error_buf);
  }

  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);

  return self;
}

static mrb_value
mrb_mbedtls_pkey_rsa_public_key(mrb_state *mrb, mrb_value self) {
  mbedtls_pk_context *orig_pk = (mbedtls_pk_context *)mrb_data_get_ptr(mrb, self, &mrb_pkey_type);
  mrb_value new_obj = mrb_obj_new(mrb, mrb_class_ptr(self), 0, NULL);
  mbedtls_pk_context *new_pk = (mbedtls_pk_context *)mrb_malloc(mrb, sizeof(mbedtls_pk_context));
  DATA_PTR(new_obj) = new_pk;
  DATA_TYPE(new_obj) = &mrb_pkey_type;
  mbedtls_pk_init(new_pk);

  int ret = mbedtls_pk_setup(new_pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  if (ret != 0) {
    mrb_raise(mrb, class_MbedTLS_PKey_PKeyError, "Failed to setup RSA context");
  }

  mbedtls_rsa_context *orig_rsa = mbedtls_pk_rsa(*orig_pk);
  mbedtls_rsa_context *new_rsa = mbedtls_pk_rsa(*new_pk);

  if ((ret = mbedtls_mpi_copy(&new_rsa->N, &orig_rsa->N)) != 0 ||
      (ret = mbedtls_mpi_copy(&new_rsa->E, &orig_rsa->E)) != 0) {
    mrb_raise(mrb, class_MbedTLS_PKey_PKeyError, "Failed to copy public key components");
  }

  // set zero to private key components
  mbedtls_mpi_init(&new_rsa->D);
  mbedtls_mpi_init(&new_rsa->P);
  mbedtls_mpi_init(&new_rsa->Q);
  mbedtls_mpi_init(&new_rsa->DP);
  mbedtls_mpi_init(&new_rsa->DQ);
  mbedtls_mpi_init(&new_rsa->QP);

  return new_obj;
}

static mrb_value
mrb_mbedtls_pkey_rsa_to_pem(mrb_state *mrb, mrb_value self)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)mrb_data_get_ptr(mrb, self, &mrb_pkey_type);
  unsigned char buf[4096];
  int ret;

  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
  bool has_private = (mbedtls_rsa_check_privkey(rsa) == 0);

  mrb_int len = 0;
  if (has_private) {
    ret = mbedtls_pk_write_key_pem(pk, buf, sizeof(buf));
  } else {
    ret = mbedtls_pk_write_pubkey_pem(pk, buf, sizeof(buf));
  }

  if (ret != 0) {
    mrb_raise(mrb, class_MbedTLS_PKey_PKeyError, "Failed to export key to PEM");
  }

  len = strlen((char *)buf);
  mrb_value str = mrb_str_new(mrb, (char *)buf, len);
  return str;
}

static mrb_value
mrb_mbedtls_pkey_rsa_s_new(mrb_state *mrb, mrb_value klass)
{
  mrb_value arg1;
  mrb_int exponent;
  mrb_get_args(mrb, "o|i", &arg1, &exponent);

  if (mrb_fixnum_p(arg1)) {
    return mrb_mbedtls_pkey_rsa_s_generate(mrb, klass);
  }
  if (mrb_string_p(arg1)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "argument must be String or Integer");
  }
 
  mrb_int key_len = RSTRING_LEN(arg1);
  if (0 < key_len && RSTRING_PTR(arg1)[key_len - 1] != '\n') {
    mrb_str_cat_cstr(mrb, arg1, "\n");
    key_len++;
  }

  mrb_value self = mrb_obj_new(mrb, mrb_class_ptr(klass), 0, NULL);
  mbedtls_pk_context *pk = (mbedtls_pk_context *)mrb_malloc(mrb, sizeof(mbedtls_pk_context));
  DATA_PTR(self) = pk;
  DATA_TYPE(self) = &mrb_pkey_type;

  mbedtls_pk_init(pk);

  int ret;
  ret = mbedtls_pk_parse_public_key(pk, (const unsigned char *)RSTRING_PTR(arg1), key_len + 1);
  if (ret != 0) { // retry it as a private key
    ret = mbedtls_pk_parse_key(pk, (const unsigned char *)RSTRING_PTR(arg1), key_len + 1, NULL, 0);
  }
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mrb_raisef(mrb, class_MbedTLS_PKey_PKeyError,
                    "Parsing key failed. ret=%d (0x%x): %s",
                    ret, (unsigned int)-ret, error_buf);
  }

  if (mbedtls_pk_get_type(pk) != MBEDTLS_PK_RSA) {
    mrb_raise(mrb, class_MbedTLS_PKey_PKeyError, "Key type is not RSA");
  }

  return self;
}

static mrb_value
mrb_mbedtls_pkey_pkeybase_verify(mrb_state *mrb, mrb_value self)
{
  mrb_value digest;
  mrb_value sig_str;
  mrb_value input_str;
  mrb_get_args(mrb, "oSS", &digest, &sig_str, &input_str);

  mbedtls_pk_context *pk = (mbedtls_pk_context *)mrb_data_get_ptr(mrb, self, &mrb_pkey_type);

  mbedtls_md_context_t *md_ctx = (mbedtls_md_context_t *)mrb_data_get_ptr(mrb, digest, &mrb_md_context_type);
  mbedtls_md_type_t md_type = mbedtls_md_get_type(md_ctx->md_info);

  const unsigned char *signature = (const unsigned char *)RSTRING_PTR(sig_str);
  size_t sig_len = RSTRING_LEN(sig_str);

  const unsigned char *input = (const unsigned char *)RSTRING_PTR(input_str);
  size_t input_len = RSTRING_LEN(input_str);

  size_t hash_len = mbedtls_md_get_size(mbedtls_md_info_from_type(md_type));
  unsigned char hash[hash_len];
  int ret = mbedtls_md(md_ctx->md_info, input, input_len, hash);
  if (ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Hash calculation failed");
  }

  ret = mbedtls_pk_verify(pk, md_type, hash, hash_len, signature, sig_len);

  if (ret != 0) {
#ifdef PICORUBY_DEBUG
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mrb_warn(mrb, "Verification failed: %s (ret=%d, 0x%x)\n", error_buf, ret, -ret);
    mrb_warn(mrb, "hash_len: %d, sig_len: %d\n", hash_len, sig_len);
    mrb_warn(mrb, "key_type: %d, key_size: %d\n", 
                  mbedtls_pk_get_type(pk), mbedtls_pk_get_len(pk));
    mrb_warn(mrb, "Hash: ");
    for(size_t i = 0; i < hash_len; i++) {
        mrb_warn(mrb, "%02x", hash[i]);
    }
    mrb_warn(mrb, "\n");
#endif
    return mrb_false_value();
  } else {
    return mrb_true_value();
  }
}

static mrb_value
mrb_mbedtls_pkey_pkeybase_sign(mrb_state *mrb, mrb_value self)
{
  mrb_value digest;
  mrb_value inuput;
  mrb_get_args(mrb, "oS", &digest, &inuput);
  (void)digest; // Currently unused, but may be used in the future

  int ret;
  unsigned char hash[32];
  unsigned char signature[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
  size_t sig_len;

  mbedtls_pk_context *pk = (mbedtls_pk_context *)mrb_data_get_ptr(mrb, self, &mrb_pkey_type);

  ret = mbedtls_sha256_ret((const unsigned char *)RSTRING_PTR(inuput), RSTRING_LEN(inuput), hash, 0);
  if(ret != 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to calculate SHA-256 hash");
  }

  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg; mbedtls_entropy_init(&entropy); mbedtls_ctr_drbg_init(&ctr_drbg);

  mrb_value mbedtls_pers = mrb_gv_get(mrb, MRB_GVSYM(_mbedtls_pers));
  ret = mbedtls_ctr_drbg_seed(&ctr_drbg,
                              mbedtls_entropy_func,
                              &entropy,
                              (const unsigned char *)RSTRING_PTR(mbedtls_pers),
                              RSTRING_LEN(mbedtls_pers));
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mrb_raisef(mrb, E_RUNTIME_ERROR, "Failed to seed RNG: %s", error_buf);
  }

  ret = mbedtls_pk_sign(pk,
                        MBEDTLS_MD_SHA256,
                        hash,
                        0,
                        signature,
                        &sig_len,
                        mbedtls_ctr_drbg_random,
                        &ctr_drbg);
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mrb_raisef(mrb, E_RUNTIME_ERROR, "Failed to sign: %s", error_buf);
  }

  mrb_value result = mrb_str_new(mrb, (const void *)signature, sig_len);

  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);

  return result;
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

  mrb_define_class_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(new),      mrb_mbedtls_pkey_rsa_s_new, MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(generate), mrb_mbedtls_pkey_rsa_s_generate, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(public_key),  mrb_mbedtls_pkey_rsa_public_key, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(export),      mrb_mbedtls_pkey_rsa_to_pem, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(to_pem),      mrb_mbedtls_pkey_rsa_to_pem, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(to_s),        mrb_mbedtls_pkey_rsa_to_pem, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM_Q(public),    mrb_mbedtls_pkey_rsa_public_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM_Q(private),   mrb_mbedtls_pkey_rsa_private_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MbedTLS_PKey_RSA, MRB_SYM(free),        mrb_mbedtls_pkey_rsa_free, MRB_ARGS_NONE());

  class_MbedTLS_PKey_PKeyError = mrb_define_class_under_id(mrb, module_MbedTLS_PKey, MRB_SYM(PKeyError), E_STANDARD_ERROR);
}


