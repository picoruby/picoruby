#ifndef MBEDTLS_DEFINED_H_
#define MBEDTLS_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

void gem_mbedtls_cipher_init(void *vm, void *class_MbedTLS);
void gem_mbedtls_cmac_init(void *vm, void *class_MbedTLS);
void gem_mbedtls_digest_init(void *vm, void *class_MbedTLS);
void gem_mbedtls_hmac_init(void *vm, void *class_MbedTLS);
void gem_mbedtls_pkey_init(void *vm, void *class_MbedTLS);

#ifdef __cplusplus
}
#endif

#endif /* MBEDTLS_DEFINED_H_ */

