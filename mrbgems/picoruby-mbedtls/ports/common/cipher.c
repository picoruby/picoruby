#include "cipher.h"
#include "mbedtls/cipher.h"
#include "mbedtls/aes.h"
#include "mbedtls/error.h"

typedef struct CipherInstance {
  mbedtls_cipher_context_t ctx;
  mbedtls_cipher_type_t cipher_type;
  mbedtls_operation_t operation;
  uint8_t key_len;
  uint8_t iv_len;
  bool key_set;
  bool iv_set;
} cipher_instance_t;

struct CipherSuite {
  const char *name;
  mbedtls_cipher_type_t cipher_type;
  uint8_t key_len;
  uint8_t iv_len;
};

static const struct CipherSuite cipher_suites[CIPHER_SUITES_COUNT] = {
  {"AES-128-CBC", MBEDTLS_CIPHER_AES_128_CBC, 16, 16},
  {"AES-192-CBC", MBEDTLS_CIPHER_AES_192_CBC, 24, 16},
  {"AES-256-CBC", MBEDTLS_CIPHER_AES_256_CBC, 32, 16},
  {"AES-128-GCM", MBEDTLS_CIPHER_AES_128_GCM, 16, 12},
  {"AES-192-GCM", MBEDTLS_CIPHER_AES_192_GCM, 24, 12},
  {"AES-256-GCM", MBEDTLS_CIPHER_AES_256_GCM, 32, 12}
};

bool
Mbedtls_cipher_is_cbc(int cipher_type)
{
  switch (cipher_type) {
    case MBEDTLS_CIPHER_AES_128_CBC:
    case MBEDTLS_CIPHER_AES_192_CBC:
    case MBEDTLS_CIPHER_AES_256_CBC:
      return true;
    default:
      return false;
  }
}

void
Mbedtls_cipher_type_key_iv_len(const char *cipher_name, int *cipher_type, unsigned char *key_len, unsigned char *iv_len)
{
  for (int i = 0; i < CIPHER_SUITES_COUNT; i++) {
    if (strcmp(cipher_name, cipher_suites[i].name) == 0) {
      *cipher_type = (int)cipher_suites[i].cipher_type;
      *key_len = cipher_suites[i].key_len;
      *iv_len = cipher_suites[i].iv_len;
      return;
    }
  }
  *cipher_type = (int)MBEDTLS_CIPHER_NONE;
  *key_len = 0;
  *iv_len = 0;
}

const char*
MbedTLS_cipher_cipher_name(int index)
{
  if (index < 0 || index >= CIPHER_SUITES_COUNT) {
    return NULL; // Invalid index
  }
  return cipher_suites[index].name;
}

int
MbedTLS_cipher_instance_size()
{
  return sizeof(cipher_instance_t);
}

int
MbedTLS_cipher_new(unsigned char *data, int cipher_type, unsigned char key_len, unsigned char iv_len)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;
  mbedtls_cipher_init(ctx);
  instance_data->cipher_type = (mbedtls_cipher_type_t)cipher_type;
  instance_data->operation = MBEDTLS_OPERATION_NONE;
  instance_data->key_len = key_len;
  instance_data->iv_len = iv_len;
  instance_data->key_set = false;
  instance_data->iv_set = false;

  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(cipher_type);
  int ret;
  ret = mbedtls_cipher_setup(ctx, cipher_info);
  if (ret != 0) {
    if (ret == MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA) {
      return CIPHER_BAD_INPUT_DATA;
    } else {
      return CIPHER_FAILED_TO_SETUP;
    }
  }

  return CIPHER_SUCCESS;
}

void
MbedTLS_cipher_set_encrypt_to_operation(unsigned char *data)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  instance_data->operation = MBEDTLS_ENCRYPT;
}

void
MbedTLS_cipher_set_decrypt_to_operation(unsigned char *data)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  instance_data->operation = MBEDTLS_DECRYPT;
}

unsigned char
MbedTLS_cipher_get_key_len(const unsigned char *data)
{
  const cipher_instance_t *instance_data = (const cipher_instance_t *)data;
  return instance_data->key_len;
}

int
MbedTLS_cipher_set_key(unsigned char *data, const unsigned char *key, int key_len_bits)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  if (instance_data->key_set) {
    return CIPHER_ALREADY_SET;
  }
  if (instance_data->operation == MBEDTLS_OPERATION_NONE) {
    return CIPHER_OPERATION_NOT_SET;
  }
  if ((key_len_bits / 8) != instance_data->key_len) {
    return CIPHER_INVALID_LENGTH;
  }

  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  int ret;
  ret = mbedtls_cipher_setkey(ctx, key, key_len_bits, instance_data->operation);
  if (ret != 0) {
    return CIPHER_SETKEY_FAILED;
  }
  if (Mbedtls_cipher_is_cbc(instance_data->cipher_type)) {
    ret = mbedtls_cipher_set_padding_mode(ctx, MBEDTLS_PADDING_PKCS7);
    if (ret != 0) {
      return CIPHER_SET_PADDING_MODE_FAILED;
    }
  }

  instance_data->key_set = true;
  return CIPHER_SUCCESS;
}

unsigned char
MbedTLS_cipher_get_iv_len(const unsigned char *data)
{
  const cipher_instance_t *instance_data = (const cipher_instance_t *)data;
  return instance_data->iv_len;
}

int
MbedTLS_cipher_set_iv(unsigned char *data, const unsigned char *iv, size_t iv_len)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  if (instance_data->iv_set) {
    return CIPHER_ALREADY_SET;
  }
  if (instance_data->operation == MBEDTLS_OPERATION_NONE) {
    return CIPHER_OPERATION_NOT_SET;
  }
  if (iv_len != instance_data->iv_len) {
    return CIPHER_INVALID_LENGTH;
  }

  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  int ret;
  ret = mbedtls_cipher_set_iv(ctx, iv, iv_len);
  if (ret != 0) {
    return CIPHER_SET_IV_FAILED;
  }
  ret = mbedtls_cipher_reset(ctx);
  if (ret != 0) {
    return CIPHER_RESET_FAILED;
  }

  instance_data->iv_set = true;
  return CIPHER_SUCCESS;
}

int
MbedTLS_cipher_update_ad(unsigned char *data, const unsigned char *ad, size_t ad_len)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;
  if (mbedtls_cipher_update_ad(ctx, ad, ad_len) != 0) {
    return CIPHER_UPDATE_AD_FAILED;
  }
  return CIPHER_SUCCESS;
}

int
MbedTLS_cipher_update(unsigned char *data, const unsigned char *input, size_t ilen, unsigned char *output, size_t *olen)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;
  if (mbedtls_cipher_update(ctx, input, ilen, output, olen) != 0) {
    return CIPHER_UPDATE_FAILED;
  }
  return CIPHER_SUCCESS;
}

int
MbedTLS_cipher_finish(unsigned char *data, unsigned char *output, size_t *olen, int *mbedtls_err)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;
  *mbedtls_err = mbedtls_cipher_finish(ctx, output, olen);
  if (*mbedtls_err != 0) {
    return CIPHER_FINISH_FAILED;
  }
  return CIPHER_SUCCESS;
}

void
MbedTLS_strerror(int errnum, char *buf, size_t buflen)
{
  mbedtls_strerror(errnum, buf, buflen);
}

int
MbedTLS_cipher_write_tag(unsigned char *data, unsigned char *tag, size_t *tag_len)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;
  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(instance_data->cipher_type);
  if (cipher_info == NULL) {
    return CIPHER_NOT_AEAD;
  }
  mbedtls_cipher_mode_t mode = mbedtls_cipher_info_get_mode(cipher_info);
  if (mode != MBEDTLS_MODE_GCM && mode != MBEDTLS_MODE_CCM && mode != MBEDTLS_MODE_CHACHAPOLY) {
    return CIPHER_NOT_AEAD;
  }
  if (mbedtls_cipher_write_tag(ctx, tag, *tag_len) != 0) {
    return CIPHER_WRITE_TAG_FAILED;
  }
  return CIPHER_SUCCESS;
}

int
MbedTLS_cipher_check_tag(unsigned char *data, const unsigned char *tag, size_t tag_len)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;
  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(instance_data->cipher_type);
  if (cipher_info == NULL) {
    return CIPHER_SUCCESS; // Not AEAD, no tag to check
  }
  mbedtls_cipher_mode_t mode = mbedtls_cipher_info_get_mode(cipher_info);
  if (mode != MBEDTLS_MODE_GCM && mode != MBEDTLS_MODE_CCM && mode != MBEDTLS_MODE_CHACHAPOLY) {
    return CIPHER_SUCCESS; // Not AEAD, no tag to check
  }
  int ret = mbedtls_cipher_check_tag(ctx, tag, tag_len);
  if (ret == MBEDTLS_ERR_CIPHER_AUTH_FAILED) {
    return CIPHER_AUTH_FAILED;
  }
  if (ret != 0) {
    return CIPHER_CHECK_TAG_FAILED;
  }
  return CIPHER_SUCCESS;
}
