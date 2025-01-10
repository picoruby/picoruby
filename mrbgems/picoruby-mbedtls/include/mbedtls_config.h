#ifndef PICORUBY_MBEDTLS_USE_R2P2_CONFIG
/*
 * We are not on a unix or windows platform,
 * so we must define MBEDTLS_PLATFORM_C
 */
#define MBEDTLS_PLATFORM_C

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

/* custom allocator */
#include "alloc.h"
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_CALLOC_MACRO   mrbc_raw_calloc
#define MBEDTLS_PLATFORM_FREE_MACRO     mrbc_raw_free

/* PKey */
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_OID_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define PSA_WANT_ALG_SHA_256
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_FS_IO
#define MBEDTLS_PKCS1_V15

#if defined(PICORUBY_DEBUG)
#define MBEDTLS_DEBUG_C
#define MBEDTLS_ERROR_C
#define MBEDTLS_ERROR_STRERROR_DUMMY
#endif

#endif
