#include <mrubyc.h>
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"

static mrbc_class *class_MbedTLS_PKey_PKeyError;

static void
c_mbedtls_pkey_rsa_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
 
  mrbc_value key_str = GET_ARG(1);
  const char *key = (const char *)key_str.string->data;
  size_t key_len = key_str.string->size;

  if (key_len <= 0 || key[key_len - 1] != '\n') {
    char *new_key = mrbc_alloc(vm, key_len + 2);
    if (new_key == NULL) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "memory allocation failed");
      return;
    }
    memcpy(new_key, key, key_len);
    new_key[key_len] = '\n';
    new_key[key_len + 1] = '\0';
    key = new_key;
    key_len++;
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_pk_context));
  mbedtls_pk_context *pk = (mbedtls_pk_context *)self.instance->data;
  mbedtls_pk_init(pk);

  int ret;
  ret = mbedtls_pk_parse_public_key(pk, (const unsigned char *)key, key_len + 1);

  if (ret != 0) {
    mbedtls_pk_free(pk);
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    mrbc_raisef(vm, class_MbedTLS_PKey_PKeyError, 
                "mbedtls_pk_parse_public_key failed. ret=%d (0x%x): %s", 
                ret, (unsigned int)-ret, error_buf);
    return;
  }

  if (mbedtls_pk_get_type(pk) != MBEDTLS_PK_RSA) {
    mbedtls_pk_free(pk);
    mrbc_raise(vm, class_MbedTLS_PKey_PKeyError, "Key type is not RSA");
    return;
  }

  SET_RETURN(self);
}

static void
c_mbedtls_pkey_pkeybase_verify(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_pk_context *pk = (mbedtls_pk_context *)v->instance->data;
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mbedtls_md_context_t *md_ctx = (mbedtls_md_context_t *)v[1].instance->data;
  mbedtls_md_type_t md_type = mbedtls_md_get_type(md_ctx->md_info);

  mrbc_value sig_str = GET_ARG(2);
  const unsigned char *signature = (const unsigned char *)sig_str.string->data;
  size_t sig_len = sig_str.string->size;

  mrbc_value input_str = GET_ARG(3);
  const unsigned char *input = (const unsigned char *)input_str.string->data;
  size_t input_len = input_str.string->size;

  size_t hash_len = mbedtls_md_get_size(mbedtls_md_info_from_type(md_type));
  unsigned char hash[hash_len];
  int ret = mbedtls_md(md_ctx->md_info, input, input_len, hash);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Hash calculation failed");
    return;
  }

  ret = mbedtls_pk_verify(pk, md_type, hash, hash_len, signature, sig_len);

  if (ret != 0) {
#ifdef PICORUBY_DEBUG
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    console_printf("Verification failed: %s (ret=%d, 0x%x)\n", error_buf, ret, -ret);
    console_printf("hash_len: %d, sig_len: %d\n", hash_len, sig_len);
    console_printf("key_type: %d, key_size: %d\n", 
                  mbedtls_pk_get_type(pk), mbedtls_pk_get_len(pk));
    console_printf("Hash: ");
    for(size_t i = 0; i < hash_len; i++) {
        console_printf("%02x", hash[i]);
    }
    console_printf("\n");
#endif
    SET_FALSE_RETURN();
  } else {
    SET_TRUE_RETURN();
  }
}

void
gem_mbedtls_pkey_init(mrbc_vm *vm, mrbc_class *module_MbedTLS)
{
  mrbc_class *module_MbedTLS_PKey = mrbc_define_module_under(vm, module_MbedTLS, "PKey");
  mrbc_class *class_MbedTLS_PKey_PKeyBase = mrbc_define_class_under(vm, module_MbedTLS_PKey, "PKey", mrbc_class_object);
//  mrbc_define_method(vm, class_MbedTLS_PKey_PKeyBase, "sign", c_mbedtls_pkey_pkeybase_sign);
  mrbc_define_method(vm, class_MbedTLS_PKey_PKeyBase, "verify", c_mbedtls_pkey_pkeybase_verify);

  mrbc_class *class_MbedTLS_PKey_RSA = mrbc_define_class_under(vm, module_MbedTLS_PKey, "RSA", class_MbedTLS_PKey_PKeyBase);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "new", c_mbedtls_pkey_rsa_new);

  class_MbedTLS_PKey_PKeyError = mrbc_define_class_under(vm, module_MbedTLS_PKey, "PKeyError", MRBC_CLASS(StandardError));
}

