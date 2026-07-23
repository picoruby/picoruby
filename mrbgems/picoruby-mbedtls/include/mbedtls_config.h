/*
 * We are not on a unix or windows platform,
 * so we must define MBEDTLS_PLATFORM_C
 */
#define MBEDTLS_PLATFORM_C

/*
 * R2P2 uses TLS over TCP; DTLS stays disabled to reduce TLS state.
 * Do not try to define MBEDTLS_SSL_DTLS_CONNECTION_ID_COMPAT here to
 * silence the -Wundef warning in ssl.h; config_adjust_ssl.h #undefs it
 * while MBEDTLS_SSL_PROTO_DTLS is disabled, so the definition is dead.
 */

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
#ifdef PICORB_DEBUG
#define MBEDTLS_DEBUG_C
#endif

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
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
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
#define MBEDTLS_GENPRIME

/* Required for ENTROPY_C when NO_PLATFORM_ENTROPY is defined */
#define MBEDTLS_ENTROPY_FORCE_SHA256

#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_HARDWARE_ALT
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_PLATFORM_MS_TIME_ALT
#define MBEDTLS_AES_FEWER_TABLES
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_PROTO_TLS1_2

/* Required for SSL_TLS_C */
#define MBEDTLS_MD5_C
#define MBEDTLS_SSL_ALPN
// #define MBEDTLS_SSL_MAX_FRAGMENT_LENGTH

/*
 * Keep buffers below the mbedTLS defaults (16KB each) while leaving enough
 * room for common HTTPS certificate records. Outgoing HTTP requests are small.
 */
#define MBEDTLS_SSL_IN_CONTENT_LEN  8192
#define MBEDTLS_SSL_OUT_CONTENT_LEN 2048

#define MBEDTLS_ERROR_C

#ifdef PICORB_PLATFORM_POSIX
/* POSIX-specific configuration for network sockets */
#define MBEDTLS_NET_C
#endif
