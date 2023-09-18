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
