#ifndef CIPHER_DEFINED_H_
#define CIPHER_DEFINED_H_

#include <stdbool.h>
#include <string.h>
#include "mbedtls_config.h"
#include "mbedtls/cipher.h"
#include "mbedtls/aes.h"

#define CIPHER_SUITES_COUNT 6

enum {
  CIPHER_NEW_SUCCESS = 0,
  CIPHER_NEW_BAD_INPUT_DATA,
  CIPHER_NEW_FAILED_TO_SETUP,
};

enum {
  CIPHER_KEY_EQ_SUCCESS = 0,
  CIPHER_KEY_EQ_OPERATION_NOT_SET,
  CIPHER_KEY_EQ_INVALID_KEY_LENGTH,
  CIPHER_KEY_EQ_FAILED_TO_SET_KEY,
  CIPHER_KEY_EQ_FAILED_TO_SET_PADDING_MODE,
  CIPHER_KEY_EQ_DOUBLE,
};

enum {
  CIPHER_IV_EQ_SUCCESS = 0,
  CIPHER_IV_EQ_OPERATION_NOT_SET,
  CIPHER_IV_EQ_INVALID_IV_LENGTH,
  CIPHER_IV_EQ_FAILED_TO_SET_IV,
  CIPHER_IV_EQ_FAILED_TO_RESET,
  CIPHER_IV_EQ_DOUBLE,
};

enum {
  CIPHER_UPDATE_AD_SUCCESS = 0,
  CIPHER_UPDATE_AD_FAILED,
};

enum {
  CIPHER_UPDATE_SUCCESS = 0,
  CIPHER_UPDATE_FAILED,
};

enum {
  CIPHER_FINISH_SUCCESS = 0,
  CIPHER_FINISH_FAILED,
};

enum {
  CIPHER_WRITE_TAG_SUCCESS = 0,
  CIPHER_WRITE_TAG_FAILED,
};

enum {
  CIPHER_CHECK_TAG_SUCCESS = 0,
  CIPHER_CHECK_TAG_FAILED,
  CIPHER_CHECK_TAG_AUTH_FAILED,
};

bool MbedTLS_cipher_is_cbc(mbedtls_cipher_type_t cipher_type);
void MbedTLS_cipher_type_key_iv_len(const char *cipher_name, mbedtls_cipher_type_t *cipher_type, uint8_t *key_len, uint8_t *iv_len);
int MbedTLS_cipher_instance_size();

int MbedTLS_cipher_new(uint8_t *data, mbedtls_cipher_type_t cipher_type, mbedtls_operation_t operation, uint8_t key_len, uint8_t iv_len);
const char* MbedTLS_cipher_cipher_name(int index);
void MbedTLS_cipher_encrypt(uint8_t *data);
void MbedTLS_cipher_decrypt(uint8_t *data);
uint8_t MbedTLS_cipher_key_len(uint8_t *data);
int MbedTLS_cipher_key_eq(uint8_t *data, const char *key, size_t key_len);
uint8_t MbedTLS_cipher_iv_len(uint8_t *data);
int MbedTLS_cipher_iv_eq(uint8_t *data, const char *iv, size_t iv_len);
int MbedTLS_cipher_update_ad(uint8_t *data, const char *input, size_t input_len);
int MbedTLS_cipher_update(uint8_t *data, const char *input, size_t input_len, unsigned char *output, size_t *output_len);
int MbedTLS_cipher_finish(uint8_t *data, unsigned char *output, size_t *output_len);
int MbedTLS_cipher_write_tag(uint8_t *data, const char *tag, size_t tag_len);
int MbedTLS_cipher_check_tag(uint8_t *data, const char *input, size_t input_len);

#endif /* CIPHER_DEFINED_H_ */
