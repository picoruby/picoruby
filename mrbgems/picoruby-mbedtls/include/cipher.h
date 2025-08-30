#ifndef CIPHER_DEFINED_H_
#define CIPHER_DEFINED_H_

#include <stdbool.h>
#include <string.h>

#define CIPHER_SUITES_COUNT 6

enum {
  CIPHER_SUCCESS = 0,
  CIPHER_BAD_INPUT_DATA,
  CIPHER_FAILED_TO_SETUP,
  CIPHER_ALREADY_SET,
  CIPHER_OPERATION_NOT_SET,
  CIPHER_INVALID_LENGTH,
  CIPHER_UPDATE_AD_FAILED,
  CIPHER_UPDATE_FAILED,
  CIPHER_FINISH_FAILED,
  CIPHER_NOT_AEAD,
  CIPHER_WRITE_TAG_FAILED,
  CIPHER_AUTH_FAILED,
  CIPHER_CHECK_TAG_FAILED,
  CIPHER_SETKEY_FAILED,
  CIPHER_SET_IV_FAILED,
  CIPHER_SET_PADDING_MODE_FAILED,
  CIPHER_RESET_FAILED
};

bool Mbedtls_cipher_is_cbc(int cipher_type);
void Mbedtls_cipher_type_key_iv_len(const char *cipher_name, int *cipher_type, unsigned char *key_len, unsigned char *iv_len);
const char* MbedTLS_cipher_cipher_name(int index);

int MbedTLS_cipher_instance_size();
int MbedTLS_cipher_new(unsigned char *data, int cipher_type, unsigned char key_len, unsigned char iv_len);
void MbedTLS_cipher_set_encrypt_to_operation(unsigned char *data);
void MbedTLS_cipher_set_decrypt_to_operation(unsigned char *data);
unsigned char MbedTLS_cipher_get_key_len(const unsigned char *data);
int MbedTLS_cipher_set_key(unsigned char *data, const unsigned char *key, int key_len_bits);
unsigned char MbedTLS_cipher_get_iv_len(const unsigned char *data);
int MbedTLS_cipher_set_iv(unsigned char *data, const unsigned char *iv, size_t iv_len);
int MbedTLS_cipher_update_ad(unsigned char *data, const unsigned char *ad, size_t ad_len);
int MbedTLS_cipher_update(unsigned char *data, const unsigned char *input, size_t ilen, unsigned char *output, size_t *olen);
int MbedTLS_cipher_finish(unsigned char *data, unsigned char *output, size_t *olen, int *mbedtls_err);
void MbedTLS_strerror(int errnum, char *buf, size_t buflen);
int MbedTLS_cipher_write_tag(unsigned char *data, unsigned char *tag, size_t *tag_len);
int MbedTLS_cipher_check_tag(unsigned char *data, const unsigned char *tag, size_t tag_len);

#endif /* CIPHER_DEFINED_H_ */
