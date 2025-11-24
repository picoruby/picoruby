#include "cmac.h"
#include "mbedtls/cmac.h"
#include "mbedtls/error.h"

typedef struct CmacInstance {
  mbedtls_cipher_context_t ctx;
} cmac_instance_t;

int
MbedTLS_cmac_instance_size()
{
  return sizeof(cmac_instance_t);
}

int
MbedTLS_cmac_init_aes(unsigned char *data, const unsigned char *key, int key_bitlen)
{
  cmac_instance_t *instance_data = (cmac_instance_t *)data;
  mbedtls_cipher_init(&instance_data->ctx);
  const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
  int ret;
  ret = mbedtls_cipher_setup(&instance_data->ctx, cipher_info);
  if (ret != 0) {
    if (ret == MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA) {
      return CMAC_BAD_INPUT_DATA;
    } else {
      return CMAC_FAILED_TO_SETUP;
    }
  }
  ret = mbedtls_cipher_cmac_starts(&instance_data->ctx, key, key_bitlen);
  if (ret != 0) {
    return CMAC_STARTS_FAILED;
  }
  return CMAC_SUCCESS;
}

int
MbedTLS_cmac_update(unsigned char *data, const unsigned char *input, int input_len)
{
  cmac_instance_t *instance_data = (cmac_instance_t *)data;
  int ret = mbedtls_cipher_cmac_update(&instance_data->ctx, input, input_len);
  if (ret != 0) {
    return CMAC_UPDATE_FAILED;
  }
  return CMAC_SUCCESS;
}

int
MbedTLS_cmac_reset(unsigned char *data)
{
  cmac_instance_t *instance_data = (cmac_instance_t *)data;
  int ret = mbedtls_cipher_cmac_reset(&instance_data->ctx);
  if (ret != 0) {
    return CMAC_RESET_FAILED;
  }
  return CMAC_SUCCESS;
}

int
MbedTLS_cmac_finish(unsigned char *data, unsigned char *output)
{
  cmac_instance_t *instance_data = (cmac_instance_t *)data;
  int ret = mbedtls_cipher_cmac_finish(&instance_data->ctx, output);
  if (ret != 0) {
    return CMAC_FINISH_FAILED;
  }
  return CMAC_SUCCESS;
}

void
MbedTLS_cmac_free(unsigned char *data)
{
  cmac_instance_t *instance_data = (cmac_instance_t *)data;
  mbedtls_cipher_free(&instance_data->ctx);
}
