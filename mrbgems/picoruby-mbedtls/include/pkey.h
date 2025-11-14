#ifndef PKEY_DEFINED_H_
#define PKEY_DEFINED_H_

#include <stdlib.h>
#include <stdbool.h>

// Error codes can be mbedtls errors (negative) or custom positive ones
enum {
  PKEY_SUCCESS = 0,
  PKEY_ERR_RNG_SEED = 1,
  PKEY_ERR_KEY_TYPE_NOT_RSA = 2,
  PKEY_CREATE_MD_FAILED = 3,
  PKEY_VERIFY_FAILED = 4,
  PKEY_SIGN_FAILED = 5,
};
#define PKEY_SIGNATURE_MAX_SIZE (1024)

int MbedTLS_pkey_instance_size(void);
void MbedTLS_pkey_init(void *ctx);
void MbedTLS_pkey_free(void *ctx);

bool MbedTLS_pkey_is_public(void *ctx);
bool MbedTLS_pkey_is_private(void *ctx);

int MbedTLS_pkey_generate_rsa(void *ctx, int bits, long exponent, const unsigned char *pers, size_t pers_len);
int MbedTLS_pkey_from_pem(void *ctx, const unsigned char *pem, size_t pem_len);
int MbedTLS_pkey_to_pem(void *ctx, unsigned char *buf, size_t size);
int MbedTLS_pkey_get_public_key(void *pub_ctx, void *prv_ctx);

int MbedTLS_pkey_verify(void *ctx, const unsigned char *md_ctx, const unsigned char *input, size_t input_len, const unsigned char *sig, size_t sig_len);
int MbedTLS_pkey_sign(void *ctx, const unsigned char *md_ctx, const unsigned char *input, size_t input_len, unsigned char *sig, size_t sig_size, size_t *sig_len, const unsigned char *pers, size_t pers_len);

void MbedTLS_pkey_strerror(int ret, char *buf, size_t buflen);

#endif /* PKEY_DEFINED_H_ */
