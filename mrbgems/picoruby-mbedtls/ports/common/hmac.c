#include "hmac.h"
#include "mbedtls/md.h"
#include <string.h>

typedef struct HmacInstance {
  mbedtls_md_context_t ctx;
} hmac_instance_t;

int
MbedTLS_hmac_instance_size()
{
  return sizeof(hmac_instance_t);
}

int
MbedTLS_hmac_init(void *data, const char *algorithm, const unsigned char *key, int key_len)
{
  if (strcmp(algorithm, "sha256") != 0) {
    return HMAC_UNSUPPORTED_HASH;
  }
  hmac_instance_t *instance_data = (hmac_instance_t *)data;
  mbedtls_md_init(&instance_data->ctx);
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  int ret;
  ret = mbedtls_md_setup(&instance_data->ctx, md_info, 1); // 1 for HMAC
  if (ret != 0) {
    return HMAC_SETUP_FAILED;
  }
  ret = mbedtls_md_hmac_starts(&instance_data->ctx, key, key_len);
  if (ret != 0) {
    return HMAC_STARTS_FAILED;
  }
  return HMAC_SUCCESS;
}

int
MbedTLS_hmac_check_finished(void *data)
{
  hmac_instance_t *instance_data = (hmac_instance_t *)data;
  if (mbedtls_md_info_from_ctx(&instance_data->ctx) == NULL) { // mbedtls_md_free() makes md_info NULL
    return HMAC_ALREADY_FINISHED;
  }
  return HMAC_SUCCESS;
}

int
MbedTLS_hmac_update(void *data, const unsigned char *input, int input_len)
{
  hmac_instance_t *instance_data = (hmac_instance_t *)data;
  if (MbedTLS_hmac_check_finished(data) != HMAC_SUCCESS) {
    return HMAC_ALREADY_FINISHED;
  }
  int ret = mbedtls_md_hmac_update(&instance_data->ctx, input, input_len);
  if (ret != 0) {
    return HMAC_UPDATE_FAILED;
  }
  return HMAC_SUCCESS;
}

int
MbedTLS_hmac_reset(void *data)
{
  hmac_instance_t *instance_data = (hmac_instance_t *)data;
  int ret = mbedtls_md_hmac_reset(&instance_data->ctx);
  if (ret != 0) {
    return HMAC_RESET_FAILED;
  }
  return HMAC_SUCCESS;
}

int
MbedTLS_hmac_finish(void *data, unsigned char *output)
{
  hmac_instance_t *instance_data = (hmac_instance_t *)data;
  if (MbedTLS_hmac_check_finished(data) != HMAC_SUCCESS) {
    return HMAC_ALREADY_FINISHED;
  }
  int ret = mbedtls_md_hmac_finish(&instance_data->ctx, output);
  if (ret != 0) {
    return HMAC_FINISH_FAILED;
  }
  return HMAC_SUCCESS;
}

void
MbedTLS_hmac_free(void *data)
{
  hmac_instance_t *instance_data = (hmac_instance_t *)data;
  mbedtls_md_free(&instance_data->ctx);
}
