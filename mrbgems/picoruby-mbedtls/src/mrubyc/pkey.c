#include <stdbool.h>
#include <string.h>
#include "mrubyc.h"

#include "pkey.h"

static mrbc_class *class_MbedTLS_PKey_PKeyError;

static void
raise_pkey_error(mrbc_vm *vm, int ret)
{
  char error_buf[100];
  MbedTLS_pkey_strerror(ret, error_buf, sizeof(error_buf));
  mrbc_raisef(vm, class_MbedTLS_PKey_PKeyError, "PKey error: %s (ret=%d)", error_buf, ret);
}

static void
c_mbedtls_pkey_rsa_public_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  void *pk = v->instance->data;
  if (MbedTLS_pkey_is_public(pk)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mbedtls_pkey_rsa_private_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  void *pk = v->instance->data;
  if (MbedTLS_pkey_is_private(pk)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mbedtls_pkey_rsa_free(mrbc_vm *vm, mrbc_value *v, int argc)
{
  void *pk = v->instance->data;
  if (pk) {
    MbedTLS_pkey_free(pk);
    // The instance itself is freed by GC
  }
  SET_NIL_RETURN();
}

static void
c_mbedtls_pkey_rsa_generate(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 1 || argc > 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_ARG(1).tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  int bits = GET_INT_ARG(1);
  int exponent = 65537;
  if (argc == 2 && GET_ARG(2).tt == MRBC_TT_INTEGER) {
    exponent = GET_INT_ARG(2);
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, MbedTLS_pkey_instance_size());
  void *pk = self.instance->data;
  MbedTLS_pkey_init(pk);

  mrbc_value *mbedtls_pers = mrbc_get_global(mrbc_str_to_symid("$_mbedtls_pers"));
  int ret = MbedTLS_pkey_generate_rsa(pk, bits, exponent, (const unsigned char *)mbedtls_pers->string->data, mbedtls_pers->string->size);

  if (ret != 0) {
    MbedTLS_pkey_free(pk);
    raise_pkey_error(vm, ret);
    return;
  }

  SET_RETURN(self);
}

static void
c_mbedtls_pkey_rsa_public_key(mrbc_vm *vm, mrbc_value *v, int argc)
{
  void *orig_pk = v->instance->data;

  mrbc_value new_obj = mrbc_instance_new(vm, v->instance->cls, MbedTLS_pkey_instance_size());
  void *new_pk = new_obj.instance->data;
  MbedTLS_pkey_init(new_pk);

  int ret = MbedTLS_pkey_get_public_key(new_pk, orig_pk);
  if (ret != 0) {
    MbedTLS_pkey_free(new_pk);
    raise_pkey_error(vm, ret);
    return;
  }

  SET_RETURN(new_obj);
}

static void
c_mbedtls_pkey_rsa_to_pem(mrbc_vm *vm, mrbc_value *v, int argc)
{
  void *pk = v->instance->data;
  unsigned char buf[4096];

  int ret = MbedTLS_pkey_to_pem(pk, buf, sizeof(buf));
  if (ret != 0) {
    raise_pkey_error(vm, ret);
    return;
  }

  mrbc_value pem = mrbc_string_new_cstr(vm, (const char *)buf);
  SET_RETURN(pem);
}

static void
c_mbedtls_pkey_rsa_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value arg1 = GET_ARG(1);

  if (arg1.tt == MRBC_TT_INTEGER) {
    c_mbedtls_pkey_rsa_generate(vm, v, argc);
    return;
  }
  if (arg1.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "argument must be String or Integer");
    return;
  }

  // mbedtls expects a null-terminated string for PEM parsing
  char *pem_cstr = mrbc_alloc(vm, arg1.string->size + 1);
  memcpy(pem_cstr, arg1.string->data, arg1.string->size);
  pem_cstr[arg1.string->size] = '\0';

  mrbc_value self = mrbc_instance_new(vm, v->cls, MbedTLS_pkey_instance_size());
  void *pk = self.instance->data;
  MbedTLS_pkey_init(pk);

  int ret = MbedTLS_pkey_from_pem(pk, (const unsigned char *)pem_cstr, strlen(pem_cstr) + 1);
  mrbc_free(vm, pem_cstr);

  if (ret != 0) {
    MbedTLS_pkey_free(pk);
    if (ret == PKEY_ERR_KEY_TYPE_NOT_RSA) {
      mrbc_raise(vm, class_MbedTLS_PKey_PKeyError, "Key type is not RSA");
    } else {
      raise_pkey_error(vm, ret);
    }
    return;
  }

  SET_RETURN(self);
}

static void
c_mbedtls_pkey_pkeybase_verify(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  void *pk = v->instance->data;
  mrbc_value digest_obj = GET_ARG(1);
  mrbc_value sig_str = GET_ARG(2);
  mrbc_value input_str = GET_ARG(3);

  // This part is tricky because we need the md_context from another object.
  // Assuming the digest object is compatible.
  const unsigned char *md_ctx = digest_obj.instance->data;

  int ret = MbedTLS_pkey_verify(pk, md_ctx, (const unsigned char *)input_str.string->data, input_str.string->size, (const unsigned char *)sig_str.string->data, sig_str.string->size);
  if (ret == PKEY_CREATE_MD_FAILED) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Hash calculation failed");
  } else if (ret == PKEY_SUCCESS) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mbedtls_pkey_pkeybase_sign(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  void *pk = v->instance->data;
  mrbc_value digest_obj = GET_ARG(1);
  mrbc_value input_str = GET_ARG(2);

  const unsigned char *md_ctx = digest_obj.instance->data;
  unsigned char sig[PKEY_SIGNATURE_MAX_SIZE];
  size_t sig_len;
  mrbc_value *mbedtls_pers = mrbc_get_global(mrbc_str_to_symid("$_mbedtls_pers"));

  int ret = MbedTLS_pkey_sign(pk, md_ctx, (const unsigned char *)input_str.string->data, input_str.string->size, sig, sizeof(sig), &sig_len, (const unsigned char *)mbedtls_pers->string->data, mbedtls_pers->string->size);
  if (ret == PKEY_CREATE_MD_FAILED) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Failed to calculate hash");
  } else if (ret != 0) {
    raise_pkey_error(vm, ret);
    return;
  }

  mrbc_value result = mrbc_string_new(vm, (const char *)sig, sig_len);
  SET_RETURN(result);
}

void
gem_mbedtls_pkey_init(mrbc_vm *vm, mrbc_class *module_MbedTLS)
{
  mrbc_class *module_MbedTLS_PKey = mrbc_define_module_under(vm, module_MbedTLS, "PKey");
  mrbc_class *class_MbedTLS_PKey_PKeyBase = mrbc_define_class_under(vm, module_MbedTLS_PKey, "PKeyBase", mrbc_class_object);
  mrbc_define_method(vm, class_MbedTLS_PKey_PKeyBase, "sign", c_mbedtls_pkey_pkeybase_sign);
  mrbc_define_method(vm, class_MbedTLS_PKey_PKeyBase, "verify", c_mbedtls_pkey_pkeybase_verify);

  mrbc_class *class_MbedTLS_PKey_RSA = mrbc_define_class_under(vm, module_MbedTLS_PKey, "RSA", class_MbedTLS_PKey_PKeyBase);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "new", c_mbedtls_pkey_rsa_new);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "generate", c_mbedtls_pkey_rsa_generate);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "public_key", c_mbedtls_pkey_rsa_public_key);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "export", c_mbedtls_pkey_rsa_to_pem);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "to_pem", c_mbedtls_pkey_rsa_to_pem);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "to_s", c_mbedtls_pkey_rsa_to_pem);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "public?", c_mbedtls_pkey_rsa_public_q);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "private?", c_mbedtls_pkey_rsa_private_q);
  mrbc_define_method(vm, class_MbedTLS_PKey_RSA, "free", c_mbedtls_pkey_rsa_free);

  class_MbedTLS_PKey_PKeyError = mrbc_define_class_under(vm, module_MbedTLS_PKey, "PKeyError", MRBC_CLASS(StandardError));
}
