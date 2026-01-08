#include "pkey.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/rsa.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include <string.h>

int
MbedTLS_pkey_instance_size(void)
{
  return sizeof(mbedtls_pk_context);
}

void
MbedTLS_pkey_init(void *ctx)
{
  mbedtls_pk_init((mbedtls_pk_context *)ctx);
}

void
MbedTLS_pkey_free(void *ctx)
{
  mbedtls_pk_free((mbedtls_pk_context *)ctx);
}

bool
MbedTLS_pkey_is_public(void *ctx)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)ctx;
  if (mbedtls_pk_get_type(pk) != MBEDTLS_PK_RSA) return false;
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
  return mbedtls_rsa_check_pubkey(rsa) == 0;
}

bool
MbedTLS_pkey_is_private(void *ctx)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)ctx;
  if (mbedtls_pk_get_type(pk) != MBEDTLS_PK_RSA) return false;
  mbedtls_rsa_context *rsa = mbedtls_pk_rsa(*pk);
  return mbedtls_rsa_check_privkey(rsa) == 0;
}

static int
setup_rng(mbedtls_ctr_drbg_context *ctr_drbg, mbedtls_entropy_context *entropy, const unsigned char *pers, size_t pers_len)
{
  mbedtls_entropy_init(entropy);
  mbedtls_ctr_drbg_init(ctr_drbg);
  int ret = mbedtls_ctr_drbg_seed(ctr_drbg, mbedtls_entropy_func, entropy, pers, pers_len);
  if (ret != 0) {
    mbedtls_ctr_drbg_free(ctr_drbg);
    mbedtls_entropy_free(entropy);
    return PKEY_ERR_RNG_SEED;
  }
  return PKEY_SUCCESS;
}

int
MbedTLS_pkey_generate_rsa(void *ctx, int bits, long exponent, const unsigned char *pers, size_t pers_len)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)ctx;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  int ret;

  ret = setup_rng(&ctr_drbg, &entropy, pers, pers_len);
  if (ret != PKEY_SUCCESS) {
    return ret;
  }

  ret = mbedtls_pk_setup(pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
  if (ret != 0) {
    goto cleanup;
  }

  ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(*pk), mbedtls_ctr_drbg_random, &ctr_drbg, bits, exponent);

cleanup:
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  return ret;
}

int
MbedTLS_pkey_from_pem(void *ctx, const unsigned char *pem, size_t pem_len)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)ctx;
  int ret;

  ret = mbedtls_pk_parse_public_key(pk, pem, pem_len);
  if (ret != 0) {
    // retry as a private key
    ret = mbedtls_pk_parse_key(pk, pem, pem_len, NULL, 0, NULL, NULL);
  }
  if (ret != 0) {
    return ret;
  }

  if (mbedtls_pk_get_type(pk) != MBEDTLS_PK_RSA) {
    return PKEY_ERR_KEY_TYPE_NOT_RSA;
  }

  return PKEY_SUCCESS;
}

int
MbedTLS_pkey_to_pem(void *ctx, unsigned char *buf, size_t size)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)ctx;
  int ret;
  if (MbedTLS_pkey_is_private(pk)) {
    ret = mbedtls_pk_write_key_pem(pk, buf, size);
  } else {
    ret = mbedtls_pk_write_pubkey_pem(pk, buf, size);
  }
  return ret;
}

int
MbedTLS_pkey_get_public_key(void *pub_ctx, void *prv_ctx)
{
  unsigned char buf[2048];
  int ret = mbedtls_pk_write_pubkey_der((mbedtls_pk_context *)prv_ctx, buf, sizeof(buf));
  if (ret < 0) {
    return ret;
  }
  size_t len = ret;

  // The key is written to the end of the buffer.
  return mbedtls_pk_parse_public_key((mbedtls_pk_context *)pub_ctx, buf + sizeof(buf) - len, len);
}

int
MbedTLS_pkey_verify(void *ctx, const unsigned char *md_ctx, const unsigned char *input, size_t input_len, const unsigned char *sig, size_t sig_len)
//MbedTLS_pkey_verify(void *ctx, mbedtls_md_type_t md_alg, const unsigned char *hash, size_t hash_len, const unsigned char *sig, size_t sig_len)
{
  mbedtls_md_context_t *md_context = (mbedtls_md_context_t *)md_ctx;
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_ctx(md_context);
  mbedtls_md_type_t md_type = mbedtls_md_get_type(md_info);

  size_t hash_len = mbedtls_md_get_size(md_info);
  unsigned char hash[hash_len];
  int ret = mbedtls_md(md_info, input, input_len, hash);
  if (ret != 0) {
    return PKEY_CREATE_MD_FAILED;
  }

  ret = mbedtls_pk_verify((mbedtls_pk_context *)ctx, md_type, hash, hash_len, sig, sig_len);
  if (ret != 0) {
    return PKEY_VERIFY_FAILED;
  }

  return PKEY_SUCCESS;
}

int
MbedTLS_pkey_sign(void *ctx, const unsigned char *md_ctx, const unsigned char *input, size_t input_len, unsigned char *sig, size_t sig_size, size_t *sig_len, const unsigned char *pers, size_t pers_len)
//MbedTLS_pkey_sign(void *ctx, mbedtls_md_type_t md_alg, const unsigned char *hash, size_t hash_len, unsigned char *sig, size_t sig_size, size_t *sig_len, const unsigned char *pers, size_t pers_len)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)ctx;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  int ret;

  mbedtls_md_context_t *md_context = (mbedtls_md_context_t *)md_ctx;
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_ctx(md_context);
  mbedtls_md_type_t md_type = mbedtls_md_get_type(md_info);

  size_t hash_len = mbedtls_md_get_size(md_info);
  unsigned char hash[hash_len];
  ret = mbedtls_md(md_info, input, input_len, hash);
  if (ret != 0) {
    return PKEY_CREATE_MD_FAILED;
  }

  ret = setup_rng(&ctr_drbg, &entropy, pers, pers_len);
  if (ret != PKEY_SUCCESS) {
    return ret;
  }

  ret = mbedtls_pk_sign(pk, md_type, hash, hash_len, sig, sig_size, sig_len, mbedtls_ctr_drbg_random, &ctr_drbg);
  if (ret != 0) {
    return PKEY_SIGN_FAILED;
  }

  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
  return ret;
}

void
MbedTLS_pkey_strerror(int ret, char *buf, size_t buflen)
{
  mbedtls_strerror(ret, buf, buflen);
}
