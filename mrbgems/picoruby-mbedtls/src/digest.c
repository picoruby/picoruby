#include <mrubyc.h>
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"

mbedtls_md_type_t
c_mbedtls_digest_algorithm_name(int key)
{
  mbedtls_md_type_t ret;
  switch (key) {
    case 0x01:
      ret = MBEDTLS_MD_SHA256;
      break;
    default:
      ret = MBEDTLS_MD_NONE;
  }
  return ret;
}

static void
c_mbedtls_digest__init_ctx(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value algorithm = GET_ARG(1);
  if (algorithm.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_md_context_t));
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)self.instance->data;
  mbedtls_md_init(ctx);

  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(c_mbedtls_digest_algorithm_name(mrbc_integer(algorithm)));
  int ret;
  ret = mbedtls_md_setup(ctx, md_info, 0);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_setup failed");
    mbedtls_md_free(ctx);
    return;
  }
  ret = mbedtls_md_starts(ctx);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_starts failed");
    mbedtls_md_free(ctx);
    return;
  }

  SET_RETURN(self);
}

static void
c_mbedtls_digest_free(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)v->instance->data;
  if (ctx != NULL) {
    mbedtls_md_free(ctx);
  }
  *v->instance->data = NULL;
}

static void
c_mbedtls_digest_update(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value input = GET_ARG(1);
  if (input.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }

  int ret;
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)v->instance->data;
  ret = mbedtls_md_update(ctx, input.string->data, input.string->size);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_update failed");
    return;
  }
  mrbc_incref(&v[0]);
  SET_RETURN(*v);
}

static void
c_mbedtls_digest_finish(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)v->instance->data;

  size_t out_len = mbedtls_md_get_size(ctx->md_info);
  unsigned char* output = mrbc_alloc(vm, out_len); // need at least block size
  int ret;

  ret = mbedtls_md_finish(ctx, output);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_digest_finish failed");
    return;
  }
  mrbc_value ret_value = mrbc_string_new(vm, output, out_len);
  mrbc_free(vm, output);
  mrbc_incref(&v[0]);
  SET_RETURN(ret_value);
}

void
gem_mbedtls_digest_init(mrbc_vm *vm, mrbc_class *module_MbedTLS)
{
  mrbc_class *class_MbedTLS_Digest = mrbc_define_class_under(vm, module_MbedTLS, "Digest", mrbc_class_object);

  mrbc_define_method(vm, class_MbedTLS_Digest, "_init_ctx", c_mbedtls_digest__init_ctx);
  mrbc_define_method(vm, class_MbedTLS_Digest, "update", c_mbedtls_digest_update);
  mrbc_define_method(vm, class_MbedTLS_Digest, "finish", c_mbedtls_digest_finish);
  mrbc_define_method(vm, class_MbedTLS_Digest, "free", c_mbedtls_digest_free);
}
