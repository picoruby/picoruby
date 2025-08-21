#include <stdio.h>
#include <string.h>
#include "mrubyc.h"
#include "mbedtls/md.h"

static void
c_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (strcmp((const char *)GET_STRING_ARG(2), "sha256") != 0) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "unsupported hash algorithm");
    return;
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(mbedtls_md_context_t));
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)self.instance->data;
  mbedtls_md_init(ctx);
  const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  int ret;
  ret = mbedtls_md_setup(ctx, md_info, 1); // 1 for HMAC
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_setup failed");
    return;
  }
  mrbc_value key = GET_ARG(1);
  ret = mbedtls_md_hmac_starts(ctx, key.string->data, key.string->size);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_hmac_starts failed");
    return;
  }
  SET_RETURN(self);
}

static int
check_finished(mrbc_vm *vm, mbedtls_md_context_t *ctx)
{
  if (mbedtls_md_info_from_ctx(ctx) == NULL) { // mbedtls_md_free() makes md_info NULL
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "already finished");
    return -1;
  }
  return 0;
}

static void
c_update(mrbc_vm *vm, mrbc_value *v, int argc)
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
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)v->instance->data;
  if (check_finished(vm, ctx) != 0) {
    return;
  }
  int ret;
  ret = mbedtls_md_hmac_update(ctx, input.string->data, input.string->size);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_hmac_update failed");
    return;
  }
}

static void
c_reset(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)v->instance->data;
  int ret = mbedtls_md_hmac_reset(ctx);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_hmac_reset failed");
    return;
  }
}

static int
finish(mrbc_vm *vm, unsigned char *output, mbedtls_md_context_t *ctx)
{
  int ret;
  if (check_finished(vm, ctx) != 0) {
    return -1;
  }
  ret = mbedtls_md_hmac_finish(ctx, output);
  /*
   * Because mruby/c doesn't have a callback in mrbc_decref(),
   * we need to free the context here.
   * So `digest` and `hexdigest` methods should be called only once.
   */
  mbedtls_md_free(ctx);
  if (ret != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_hmac_finish failed");
    return -1;
  }
  return 0;
}

static void
c_digest(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)v->instance->data;
  unsigned char output[32]; // SHA256 produces 32 bytes output
  if (finish(vm, output, ctx) != 0) {
    return;
  }
  mrbc_value digest = mrbc_string_new(vm, output, sizeof(output));
  mrbc_incref(&v[0]);
  SET_RETURN(digest);
}

static void
c_hexdigest(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mbedtls_md_context_t *ctx = (mbedtls_md_context_t *)v->instance->data;
  unsigned char output[32]; // SHA256 produces 32 bytes output
  if (finish(vm, output, ctx) != 0) {
    return;
  }
  char hexdigest[64]; // 32 bytes * 2
  for (int i = 0; i < 32; i++) {
    sprintf(hexdigest + i * 2, "%02x", output[i]);
  }
  mrbc_value result = mrbc_string_new_cstr(vm, hexdigest);
  mrbc_incref(&v[0]);
  SET_RETURN(result);
}

void
gem_mbedtls_hmac_init(mrbc_vm *vm, mrbc_class *module_MbedTLS)
{
  mrbc_class *class_MbedTLS_HMAC = mrbc_define_class_under(vm, module_MbedTLS, "HMAC", mrbc_class_object);
  mrbc_define_method(vm, class_MbedTLS_HMAC, "new", c_new);
  mrbc_define_method(vm, class_MbedTLS_HMAC, "update", c_update);
  mrbc_define_method(vm, class_MbedTLS_HMAC, "reset", c_reset);
  mrbc_define_method(vm, class_MbedTLS_HMAC, "digest", c_digest);
  mrbc_define_method(vm, class_MbedTLS_HMAC, "hexdigest", c_hexdigest);
}
