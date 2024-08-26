#ifndef PICORUBY_MBEDTLS_USE_R2P2_CONFIG
/*
 * We are not on a unix or windows platform,
 * so we must define MBEDTLS_PLATFORM_C
 */
#define MBEDTLS_NO_PLATFORM_ENTROPY

/*
 * For MbedTLS::CMAC being required at least
 * MBEDTLS_CIPHER_C and ( MBEDTLS_AES_C or MBEDTLS_DES_C )
 */
#define MBEDTLS_CMAC_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C

/* This is needed for AES-CBC and AES-GCM */
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_PADDING_PKCS7
#define MBEDTLS_CIPHER_MODE_GCM
#define MBEDTLS_GCM_C

/* This is needed for SHA256 */
#define MBEDTLS_MD_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
#endif