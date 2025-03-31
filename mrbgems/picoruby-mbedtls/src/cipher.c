#include <stdbool.h>
#include <string.h>
#include "mbedtls/cipher.h"
#include "mbedtls/aes.h"

#define CIPHER_SUITES_COUNT 6

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

typedef struct CipherInstance {
  mbedtls_cipher_context_t ctx;
  mbedtls_cipher_type_t cipher_type;
  mbedtls_operation_t operation;
  uint8_t key_len;
  uint8_t iv_len;
  bool key_set;
  bool iv_set;
} cipher_instance_t;

static bool
mbedtls_cipher_is_cbc(mbedtls_cipher_type_t cipher_type)
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

static void
mbedtls_cipher_type_key_iv_len(const char *cipher_name, mbedtls_cipher_type_t *cipher_type, uint8_t *key_len, uint8_t *iv_len)
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
#if defined(PICORB_VM_MRUBY)

#include "mruby/cipher.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/cipher.c"

#endif
