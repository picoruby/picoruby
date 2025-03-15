#include <stdbool.h>
#include <mrubyc.h>
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "mbedtls/error.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/bignum.h"

static mrbc_class *class_MbedTLS_PKey_PKeyError;

static void
c_mbedtls_pkey_rsa_public_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)v->instance->data;
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
  if (rsa->N.p != NULL && rsa->E.p != NULL) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mbedtls_pkey_rsa_private_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)v->instance->data;
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
  if (rsa->D.p != NULL) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mbedtls_pkey_rsa_free(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)v->instance->data;
  if (pk == NULL) {
    return;
  }
  if (mbedtls_pk_get_type(pk) == MBEDTLS_PK_RSA) {
    mbedtls_pk_free(pk);
  }
}

static void
c_mbedtls_pkey_rsa_generate(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 1 || 2 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mrbc_value arg = GET_ARG(1);
  if (mrbc_type(arg) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "argument must be Integer");
    return;
  }

  int exponent = 65537;
  if (v[2].tt == MRBC_TT_INTEGER) {
    exponent = v[2].i;
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_pk_context));
  mbedtls_pk_context *pk = (mbedtls_pk_context *)self.instance->data;
  mbedtls_pk_init(pk);

  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  int ret = 1;

  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);
  mrbc_value *mbedtls_pers = mrbc_get_global(mrbc_str_to_symid("$_mbedtls_pers"));
  const char *pers = (const char *)mbedtls_pers->string->data;
  ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                              (const unsigned char *)pers,
                              strlen(pers));
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mrbc_raisef(vm, class_MbedTLS_PKey_PKeyError,
                "Failed to seed RNG. ret=%d (0x%x): %s",
                ret, (unsigned int)-ret, error_buf);
    goto cleanup;
  }

  ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  if (ret != 0) {
    mrbc_raisef(vm, class_MbedTLS_PKey_PKeyError, "Failed to setup RSA context");
    goto cleanup;
  }

  ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(*pk),
                           mbedtls_ctr_drbg_random,
                           &ctr_drbg,
                           mrbc_integer(arg),
                           exponent);
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mrbc_raisef(vm, class_MbedTLS_PKey_PKeyError,
                "Failed to generate RSA key. ret=%d (0x%x): %s",
                ret, (unsigned int)-ret, error_buf);
    goto cleanup;
  }

cleanup:
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);

  if (ret != 0) {
    mbedtls_pk_free(pk);
    return;
  }

  SET_RETURN(self);
}

static void
c_mbedtls_pkey_rsa_public_key(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mbedtls_pk_context *orig_pk = (mbedtls_pk_context *)v->instance->data;
  mrbc_value new_obj = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_pk_context));
  mbedtls_pk_context *new_pk = (mbedtls_pk_context *)new_obj.instance->data;
  mbedtls_pk_init(new_pk);

  int ret = mbedtls_pk_setup(new_pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  if (ret != 0) {
    mbedtls_pk_free(new_pk);
    mrbc_raise(vm, class_MbedTLS_PKey_PKeyError, "Failed to setup RSA context");
    return;
  }

  mbedtls_rsa_context *orig_rsa = mbedtls_pk_rsa(*orig_pk);
  mbedtls_rsa_context *new_rsa = mbedtls_pk_rsa(*new_pk);

  if ((ret = mbedtls_mpi_copy(&new_rsa->N, &orig_rsa->N)) != 0 ||
      (ret = mbedtls_mpi_copy(&new_rsa->E, &orig_rsa->E)) != 0) {
    mbedtls_pk_free(new_pk);
    mrbc_raise(vm, class_MbedTLS_PKey_PKeyError, "Failed to copy public key components");
    return;
  }

  // set zero to private key components
  mbedtls_mpi_init(&new_rsa->D);
  mbedtls_mpi_init(&new_rsa->P);
  mbedtls_mpi_init(&new_rsa->Q);
  mbedtls_mpi_init(&new_rsa->DP);
  mbedtls_mpi_init(&new_rsa->DQ);
  mbedtls_mpi_init(&new_rsa->QP);

  SET_RETURN(new_obj);
}

static void
c_mbedtls_pkey_rsa_to_pem(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mbedtls_pk_context *pk = (mbedtls_pk_context *)v->instance->data;
  unsigned char buf[4096];
  int ret;

  // 秘密鍵が含まれているかチェック
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
  bool has_private = (mbedtls_rsa_check_privkey(rsa) == 0);

  // PEM形式に変換
  size_t len = 0;
  if (has_private) {
    ret = mbedtls_pk_write_key_pem(pk, buf, sizeof(buf));
  } else {
    ret = mbedtls_pk_write_pubkey_pem(pk, buf, sizeof(buf));
  }

  if (ret != 0) {
    mrbc_raise(vm, class_MbedTLS_PKey_PKeyError, "Failed to export key to PEM");
    return;
  }

  len = strlen((char *)buf);
  mrbc_value str = mrbc_string_new(vm, (char *)buf, len);
  SET_RETURN(str);
}

static void
c_mbedtls_pkey_rsa_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  if (v[1].tt == MRBC_TT_INTEGER) {
    c_mbedtls_pkey_rsa_generate(vm, v, argc);
    return;
  }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "argument must be String or Integer");
    return;
  }
 
  mrbc_value key_str = GET_ARG(1);
  char *key = (char *)key_str.string->data;
  size_t key_len = key_str.string->size;

  if (0 < key_len && key[key_len - 1] != '\n') {
    key = mrbc_realloc(vm, key, key_len + 2);
    key[key_len] = '\n';
    key[key_len + 1] = '\0';
    key_len++;
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_pk_context));
  mbedtls_pk_context *pk = (mbedtls_pk_context *)self.instance->data;
  mbedtls_pk_init(pk);

  int ret;
  ret = mbedtls_pk_parse_public_key(pk, (const unsigned char *)key, key_len + 1);
  if (ret != 0) { // retry it as a private key
    ret = mbedtls_pk_parse_key(pk, (const unsigned char *)key, key_len + 1, NULL, 0);
  }
  if (ret != 0) {
    mbedtls_pk_free(pk);
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mrbc_raisef(vm, class_MbedTLS_PKey_PKeyError,
                "Parsing key failed. ret=%d (0x%x): %s",
                ret, (unsigned int)-ret, error_buf);
    return;
  }

  if (mbedtls_pk_get_type(pk) != MBEDTLS_PK_RSA) {
    mbedtls_pk_free(pk);
    mrbc_raise(vm, class_MbedTLS_PKey_PKeyError, "Key type is not RSA");
    return;
  }

  SET_RETURN(self);
}

static void
c_mbedtls_pkey_pkeybase_verify(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)v->instance->data;
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mbedtls_md_context_t *md_ctx = (mbedtls_md_context_t *)v[1].instance->data;
  mbedtls_md_type_t md_type = mbedtls_md_get_type(md_ctx->md_info);

  mrbc_value sig_str = GET_ARG(2);
  const unsigned char *signature = (const unsigned char *)sig_str.string->data;
  size_t sig_len = sig_str.string->size;

  mrbc_value input_str = GET_ARG(3);
  const unsigned char *input = (const unsigned char *)input_str.string->data;
  size_t input_len = input_str.string->size;

  size_t hash_len = mbedtls_md_get_size(mbedtls_md_info_from_type(md_type));
  unsigned char hash[hash_len];
  int ret = mbedtls_md(md_ctx->md_info, input, input_len, hash);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Hash calculation failed");
    return;
  }

  ret = mbedtls_pk_verify(pk, md_type, hash, hash_len, signature, sig_len);

  if (ret != 0) {
#ifdef PICORUBY_DEBUG
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    console_printf("Verification failed: %s (ret=%d, 0x%x)\n", error_buf, ret, -ret);
    console_printf("hash_len: %d, sig_len: %d\n", hash_len, sig_len);
    console_printf("key_type: %d, key_size: %d\n", 
                  mbedtls_pk_get_type(pk), mbedtls_pk_get_len(pk));
    console_printf("Hash: ");
    for(size_t i = 0; i < hash_len; i++) {
        console_printf("%02x", hash[i]);
    }
    console_printf("\n");
#endif
    SET_FALSE_RETURN();
  } else {
    SET_TRUE_RETURN();
  }
}

static void
c_mbedtls_pkey_pkeybase_sign(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  int ret;
  unsigned char hash[32];
  unsigned char signature[MBEDTLS_PK_SIGNATURE_MAX_SIZE];
  size_t sig_len;

  mbedtls_pk_context *pk = (mbedtls_pk_context *)v[0].instance->data;
  const uint8_t *data = v[2].string->data;
  size_t data_len = v[2].string->size;

  ret = mbedtls_sha256_ret((const unsigned char *)data, data_len, hash, 0);
  if(ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Failed to calculate SHA-256 hash");
    return;
  }

  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg; mbedtls_entropy_init(&entropy); mbedtls_ctr_drbg_init(&ctr_drbg);

  mrbc_value *mbedtls_pers = mrbc_get_global(mrbc_str_to_symid("$_mbedtls_pers"));
  const char *pers = (const char *)mbedtls_pers->string->data;
  ret = mbedtls_ctr_drbg_seed(&ctr_drbg,
                              mbedtls_entropy_func,
                              &entropy,
                              (const unsigned char *)pers,
                              strlen(pers));
  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mrbc_raisef(vm, MRBC_CLASS(RuntimeError), "Failed to seed RNG: %s", error_buf);
    goto cleanup;
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
    mrbc_raisef(vm, MRBC_CLASS(RuntimeError), "Failed to sign: %s", error_buf);
    goto cleanup;
  }

  mrbc_value result = mrbc_string_new(vm, (const void *)signature, sig_len);
  SET_RETURN(result);

cleanup:
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
}


void
gem_mbedtls_pkey_init(mrbc_vm *vm, mrbc_class *module_MbedTLS)
{
  mrbc_class *module_MbedTLS_PKey = mrbc_define_module_under(vm, module_MbedTLS, "PKey");
  mrbc_class *class_MbedTLS_PKey_PKeyBase = mrbc_define_class_under(vm, module_MbedTLS_PKey, "PKey", mrbc_class_object);
  mrbc_define_method(vm, class_MbedTLS_PKey_PKeyBase, "sign", c_mbedtls_pkey_pkeybase_sign);
  mrbc_define_method(vm, class_MbedTLS_PKey_PKeyBase, "verify", c_mbedtls_pkey_pkeybase_verify);

  mrbc_class *class_MbedTLS_PKey_RSA = mrbc_define_class_under(vm, module_MbedTLS_PKey, "RSA", class_MbedTLS_PKey_PKeyBase);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "new", c_mbedtls_pkey_rsa_new);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "generate", c_mbedtls_pkey_rsa_generate);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "public_key", c_mbedtls_pkey_rsa_public_key);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "export", c_mbedtls_pkey_rsa_to_pem);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "to_pem", c_mbedtls_pkey_rsa_to_pem);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "to_s", c_mbedtls_pkey_rsa_to_pem);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "public?", c_mbedtls_pkey_rsa_public_q);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "private?", c_mbedtls_pkey_rsa_private_q);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "free", c_mbedtls_pkey_rsa_free);

  class_MbedTLS_PKey_PKeyError = mrbc_define_class_under(vm, module_MbedTLS_PKey, "PKeyError", MRBC_CLASS(StandardError));
}

