#include <stdio.h>
#include <string.h>
#include "mrubyc.h"
#include "hmac.h"

static void
c_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, MbedTLS_hmac_instance_size());
  void *hmac_instance = self.instance->data;
  mrbc_value key = GET_ARG(1);
  const char *algorithm = (const char *)GET_STRING_ARG(2);

  int ret = MbedTLS_hmac_init(hmac_instance, algorithm, key.string->data, key.string->size);
  if (ret != HMAC_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "HMAC init failed");
    return;
  }
  SET_RETURN(self);
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
  void *hmac_instance = v->instance->data;
  int ret = MbedTLS_hmac_update(hmac_instance, input.string->data, input.string->size);
  if (ret != HMAC_SUCCESS) {
    if (ret == HMAC_ALREADY_FINISHED) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "already finished");
    } else {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "HMAC update failed");
    }
  }
}

static void
c_reset(mrbc_vm *vm, mrbc_value *v, int argc)
{
  void *hmac_instance = v->instance->data;
  int ret = MbedTLS_hmac_reset(hmac_instance);
  if (ret != HMAC_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "HMAC reset failed");
  }
}

static void
c_digest(mrbc_vm *vm, mrbc_value *v, int argc)
{
  void *hmac_instance = v->instance->data;
  unsigned char output[32]; // SHA256 produces 32 bytes output
  int ret = MbedTLS_hmac_finish(hmac_instance, output);
  if (ret != HMAC_SUCCESS) {
    if (ret == HMAC_ALREADY_FINISHED) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "already finished");
    } else {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "HMAC finish failed");
    }
    return;
  }
  MbedTLS_hmac_free(hmac_instance);
  mrbc_value digest = mrbc_string_new(vm, output, sizeof(output));
  mrbc_incref(&v[0]);
  SET_RETURN(digest);
}

static void
c_hexdigest(mrbc_vm *vm, mrbc_value *v, int argc)
{
  void *hmac_instance = v->instance->data;
  unsigned char output[32]; // SHA256 produces 32 bytes output
  int ret = MbedTLS_hmac_finish(hmac_instance, output);
  if (ret != HMAC_SUCCESS) {
    if (ret == HMAC_ALREADY_FINISHED) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "already finished");
    } else {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "HMAC finish failed");
    }
    return;
  }
  MbedTLS_hmac_free(hmac_instance);
  char hexdigest[65]; // 32 bytes * 2 + 1
  for (int i = 0; i < 32; i++) {
    sprintf(hexdigest + i * 2, "%02x", output[i]);
  }
  hexdigest[64] = '\0';
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
