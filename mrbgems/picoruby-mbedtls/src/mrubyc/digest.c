#include "mrubyc.h"
#include "digest.h"

static void
mrbc_digest_free(mrbc_value *self)
{
  MbedTLS_digest_free((unsigned char *)self->instance->data);
}

static void
c_mbedtls_digest_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value algorithm = GET_ARG(1);
  if (algorithm.tt != MRBC_TT_SYMBOL) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type of argument");
    return;
  }
  int alg = MbedTLS_digest_algorithm_name(mrbc_symid_to_str(algorithm.sym_id));
  if (alg == -1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid algorithm");
    return;
  }

  mrbc_value self = mrbc_instance_new(vm, v->cls, MbedTLS_digest_instance_size());
  unsigned char *ctx = (unsigned char *)self.instance->data;

  int ret = MbedTLS_digest_new(ctx, alg);
  if (ret == DIGEST_FAILED_TO_SETUP) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_setup failed");
    return;
  } else if (ret == DIGEST_FAILED_TO_START) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_starts failed");
    return;
  }

  SET_RETURN(self);
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

  unsigned char *ctx = (unsigned char *)v->instance->data;
  if (MbedTLS_digest_update(ctx, input.string->data, input.string->size) != 0) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "mbedtls_md_update failed");
    return;
  }
  mrbc_incref(&v[0]);
  SET_RETURN(*v);
}

static void
c_mbedtls_digest_finish(mrbc_vm *vm, mrbc_value *v, int argc)
{
  unsigned char *ctx = (unsigned char *)v->instance->data;

  size_t out_len = MbedTLS_digest_get_size(ctx);
  unsigned char* output = mrbc_alloc(vm, out_len);

  if (MbedTLS_digest_finish(ctx, output) != 0) {
    mrbc_free(vm, output);
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
  mrbc_define_destructor(class_MbedTLS_Digest, mrbc_digest_free);

  mrbc_define_method(vm, class_MbedTLS_Digest, "new", c_mbedtls_digest_new);
  mrbc_define_method(vm, class_MbedTLS_Digest, "update", c_mbedtls_digest_update);
  mrbc_define_method(vm, class_MbedTLS_Digest, "finish", c_mbedtls_digest_finish);
}
