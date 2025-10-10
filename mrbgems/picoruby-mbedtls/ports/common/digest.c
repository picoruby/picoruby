#include "digest.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include <string.h>

int
MbedTLS_digest_algorithm_name(const char * name)
{
  int ret;
  if (strcmp(name, "sha256") == 0) {
    ret = (int)MBEDTLS_MD_SHA256;
  } else if (strcmp(name, "none") == 0) {
    ret = (int)MBEDTLS_MD_NONE;
  } else {
    ret = -1;
  }
  return ret;
}

int
MbedTLS_digest_instance_size(void)
{
  return sizeof(mbedtls_md_context_t);
}

int
MbedTLS_digest_new(unsigned char *data, int alg)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)data;
  mbedtls_md_init(ctx);

  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type((mbedtls_md_type_t)alg);
  int ret;
  ret = mbedtls_md_setup(ctx, md_info, 0);
  if (ret != 0) {
    mbedtls_md_free(ctx);
    return DIGEST_FAILED_TO_SETUP;
  }
  ret = mbedtls_md_starts(ctx);
  if (ret != 0) {
    mbedtls_md_free(ctx);
    return DIGEST_FAILED_TO_START;
  }
  return DIGEST_SUCCESS;
}

int
MbedTLS_digest_update(unsigned char *data, const unsigned char *input, size_t ilen)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)data;
  return mbedtls_md_update(ctx, input, ilen);
}

size_t
MbedTLS_digest_get_size(unsigned char *data)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)data;
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_ctx(ctx);
  return mbedtls_md_get_size(md_info);
}

int
MbedTLS_digest_finish(unsigned char *data, unsigned char *output)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)data;
  return mbedtls_md_finish(ctx, output);
}

void
MbedTLS_digest_free(unsigned char *data)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)data;
  if (ctx != NULL) {
    mbedtls_md_free(ctx);
  }
}
