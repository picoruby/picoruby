#include "cipher.h"

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
MbedTLS_cipher_is_cbc(mbedtls_cipher_type_t cipher_type)
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
MbedTLS_cipher_type_key_iv_len(const char *cipher_name, mbedtls_cipher_type_t *cipher_type, uint8_t *key_len, uint8_t *iv_len)
{
  for (int i = 0; i < CIPHER_SUITES_COUNT; i++) {
    if (strcmp(cipher_name, cipher_suites[i].name) == 0) {
      *cipher_type = cipher_suites[i].cipher_type;
      *key_len = cipher_suites[i].key_len;
      *iv_len = cipher_suites[i].iv_len;
      return;
    }
  }
  *cipher_type = MBEDTLS_CIPHER_NONE;
  *key_len = 0;
  *iv_len = 0;
}

int
MbedTLS_cipher_instance_size()
{
  return sizeof(cipher_instance_t);
}

int
MbedTLS_cipher_new(uint8_t *data, mbedtls_cipher_type_t cipher_type, mbedtls_operation_t operation, uint8_t key_len, uint8_t iv_len)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;
  mbedtls_cipher_init(ctx);
  instance_data->cipher_type = cipher_type;
  instance_data->operation = operation;
  instance_data->key_len = key_len;
  instance_data->iv_len = iv_len;
  instance_data->key_set = false;
  instance_data->iv_set = false;

  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(cipher_type);
  int ret;
  ret = mbedtls_cipher_setup(ctx, cipher_info);
  if (ret != 0) {
    if (ret == MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA) {
      return CIPHER_NEW_BAD_INPUT_DATA;
    } else {
      return CIPHER_NEW_FAILED_TO_SETUP;
    }
  }

  return CIPHER_NEW_SUCCESS;
}

const char*
MbedTLS_cipher_cipher_name(int index)
{
  if (index < 0 || index >= CIPHER_SUITES_COUNT) {
    return NULL; // Invalid index
  }
  return cipher_suites[index].name;
}

void
MbedTLS_cipher_encrypt(uint8_t *data)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  instance_data->operation = MBEDTLS_ENCRYPT;
}

void
MbedTLS_cipher_decrypt(uint8_t *data)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  instance_data->operation = MBEDTLS_DECRYPT;
}

uint8_t
MbedTLS_cipher_key_len(uint8_t *data)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  return instance_data->key_len;
}

int
MbedTLS_cipher_key_eq(uint8_t *data, const char *key, size_t key_len)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  if (instance_data->key_set) {
    return CIPHER_KEY_EQ_DOUBLE;
  }
  if (instance_data->operation == MBEDTLS_OPERATION_NONE) {
    return CIPHER_KEY_EQ_OPERATION_NOT_SET;
  }
  if (key_len != instance_data->key_len) {
    return CIPHER_KEY_EQ_INVALID_KEY_LENGTH;
  }

  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  int ret;
  ret = mbedtls_cipher_setkey(ctx, (const unsigned char *)key, key_len * 8, instance_data->operation); /* last arg is keybits */
  if (ret != 0) {
    return CIPHER_KEY_EQ_FAILED_TO_SET_KEY;
  }
  if (MbedTLS_cipher_is_cbc(instance_data->cipher_type)) {
    ret = mbedtls_cipher_set_padding_mode(ctx, MBEDTLS_PADDING_PKCS7);
    if (ret != 0) {
      return CIPHER_KEY_EQ_FAILED_TO_SET_PADDING_MODE;
    }
  }

  instance_data->key_set = true;

  return CIPHER_KEY_EQ_SUCCESS;
}

uint8_t
MbedTLS_cipher_iv_len(uint8_t *data)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  return instance_data->iv_len;
}

int
MbedTLS_cipher_iv_eq(uint8_t *data, const char *iv, size_t iv_len)
{
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  if (instance_data->iv_set) {
    return CIPHER_IV_EQ_DOUBLE;
  }
  if (instance_data->operation == MBEDTLS_OPERATION_NONE) {
    return CIPHER_IV_EQ_OPERATION_NOT_SET;
  }
  if (iv_len != instance_data->iv_len) {
    return CIPHER_IV_EQ_INVALID_IV_LENGTH;
  }

  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  int ret;
  ret = mbedtls_cipher_set_iv(ctx, (const unsigned char *)iv, iv_len);
  if (ret != 0) {
    return CIPHER_IV_EQ_FAILED_TO_SET_IV;
  }
  ret = mbedtls_cipher_reset(ctx);
  if (ret != 0) {
    return CIPHER_IV_EQ_FAILED_TO_RESET;
  }

  instance_data->iv_set = true;

  return CIPHER_IV_EQ_SUCCESS;
}

int
MbedTLS_cipher_update_ad(uint8_t *data, const char *input, size_t input_len)
{
  int ret;
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  ret = mbedtls_cipher_update_ad(ctx, (const unsigned char *)input, input_len);
  if (ret != 0) {
    return CIPHER_UPDATE_AD_FAILED;
  }

  return CIPHER_UPDATE_AD_SUCCESS;
}

int
MbedTLS_cipher_update(uint8_t *data, const char *input, size_t input_len, unsigned char *output, size_t *output_len)
{
  int ret;
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  ret = mbedtls_cipher_update(ctx, (const unsigned char *)input, input_len, output, output_len);
  if (ret != 0) {
    return CIPHER_UPDATE_FAILED;
  }

  return CIPHER_UPDATE_SUCCESS;
}

int
MbedTLS_cipher_finish(uint8_t *data, unsigned char *output, size_t *output_len)
{
  int ret;
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  ret = mbedtls_cipher_finish(ctx, output, output_len);
  if (ret != 0) {
    return CIPHER_FINISH_FAILED;
  }

  return CIPHER_FINISH_SUCCESS;
}

int
MbedTLS_cipher_write_tag(uint8_t *data, const char *tag, size_t tag_len)
{
  int ret;

  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  ret = mbedtls_cipher_write_tag(ctx, (unsigned char *)tag, tag_len);
  if (ret != 0) {
    return CIPHER_WRITE_TAG_FAILED;
  }

  return CIPHER_WRITE_TAG_SUCCESS;
}

int
MbedTLS_cipher_check_tag(uint8_t *data, const char *input, size_t input_len)
{
  int ret;
  cipher_instance_t *instance_data = (cipher_instance_t *)data;
  mbedtls_cipher_context_t *ctx = &instance_data->ctx;

  ret = mbedtls_cipher_check_tag(ctx, (const unsigned char *)input, input_len);
  if (ret == MBEDTLS_ERR_CIPHER_AUTH_FAILED) {
    return CIPHER_CHECK_TAG_AUTH_FAILED;
  } else if (ret != 0) {
    return CIPHER_CHECK_TAG_FAILED;
  }

  return CIPHER_CHECK_TAG_SUCCESS;
}
