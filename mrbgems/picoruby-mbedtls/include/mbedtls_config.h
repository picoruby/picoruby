/*
 * We are not on a unix or windows platform,
 * so we must define MBEDTLS_PLATFORM_C
 */
#define MBEDTLS_PLATFORM_C

/* DTLS Connection ID feature and compatibility setting for MbedTLS v3 */
#define MBEDTLS_SSL_DTLS_CONNECTION_ID
#define MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT 0

// #define MBEDTLS_TIMING_C
// #define MBEDTLS_TIMING_ALT

/*
* To debug TSL connection, you can use the following code:
* ```
* mbedtls_ssl_conf_dbg((mbedtls_ssl_config *)tls_config, mbedtls_debug, NULL);
* mbedtls_debug_set_threshold(5); # 1-5. Greater values will print more information.
* ```
* MBEDTLS_DEBUG_C is required for this.
*/
// #define MBEDTLS_DEBUG_C

// See mbedtls/include/mbedtls/config.h about MBEDTLS_ERROR_STRERROR_DUMMY
// #define MBEDTLS_ERROR_STRERROR_DUMMY

/*
 * For MbedTLS::CMAC being required at least
 * MBEDTLS_CIPHER_C and ( MBEDTLS_AES_C or MBEDTLS_DES_C )
 */
#define MBEDTLS_CMAC_C
#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C

/* For ECDH */
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP192R1_ENABLED
#define MBEDTLS_ECP_DP_SECP224R1_ENABLED
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP521R1_ENABLED
#define MBEDTLS_ECP_DP_SECP192K1_ENABLED
#define MBEDTLS_ECP_DP_SECP224K1_ENABLED
#define MBEDTLS_ECP_DP_SECP256K1_ENABLED
#define MBEDTLS_ECP_DP_BP256R1_ENABLED
#define MBEDTLS_ECP_DP_BP384R1_ENABLED
#define MBEDTLS_ECP_DP_BP512R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_X509_USE_C

/* Required for ECDHE key exchanges */
#define MBEDTLS_ECDH_LEGACY_CONTEXT

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
//#define MBEDTLS_SHA256_ALT
#define MBEDTLS_SHA512_C

/* custom allocator */
#define MBEDTLS_PLATFORM_MEMORY
#if defined(PICORB_VM_MRUBY)
# define MBEDTLS_PLATFORM_CALLOC_MACRO   calloc
# define MBEDTLS_PLATFORM_FREE_MACRO     free
#elif defined(PICORB_VM_MRUBYC)
# include "alloc.h"
# define MBEDTLS_PLATFORM_CALLOC_MACRO   mrbc_raw_calloc
# define MBEDTLS_PLATFORM_FREE_MACRO     mrbc_raw_free
#endif

/* PKey */
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_PEM_WRITE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_OID_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_GENPRIME

/* Required for ENTROPY_C when NO_PLATFORM_ENTROPY is defined */
#define MBEDTLS_ENTROPY_FORCE_SHA256

//#if !defined(PICORB_PLATFORM_POSIX)
  #define MBEDTLS_NO_PLATFORM_ENTROPY
  #define MBEDTLS_ENTROPY_HARDWARE_ALT
  #define MBEDTLS_HAVE_TIME
  #define MBEDTLS_PLATFORM_MS_TIME_ALT
  #define MBEDTLS_KEY_EXCHANGE_RSA_ENABLED
  #define MBEDTLS_CCM_C
  #define MBEDTLS_AES_FEWER_TABLES
  #define MBEDTLS_SSL_TLS_C
  #define MBEDTLS_SSL_CLI_C
  #define MBEDTLS_SSL_SERVER_NAME_INDICATION
  #define MBEDTLS_SSL_PROTO_TLS1_2
  #define MBEDTLS_SSL_PROTO_DTLS

  /* Required for SSL_TLS_C */
  #define MBEDTLS_MD5_C
#define MBEDTLS_SSL_ALPN
// #define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH
//#endif

#define MBEDTLS_ERROR_C

#ifdef PICORB_PLATFORM_POSIX
/* POSIX-specific configuration for network sockets */
#define MBEDTLS_NET_C
#endif
