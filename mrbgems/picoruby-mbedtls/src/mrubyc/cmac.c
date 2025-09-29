#include "mrubyc.h"
#include "cmac.h"

static void
c__init_aes(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, MbedTLS_cmac_instance_size());
  mrbc_value key = GET_ARG(1);
  int ret = MbedTLS_cmac_init_aes(self.instance->data, key.string->data, key.string->size * 8);
  if (ret != CMAC_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CMAC init failed");
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
  uint8_t *cmac_instance = v->instance->data;
  int ret;
  ret = MbedTLS_cmac_update(cmac_instance, input.string->data, input.string->size);
  if (ret != CMAC_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CMAC update failed");
    return;
  }
  SET_RETURN(*v);
}

static void
c_reset(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *cmac_instance = v->instance->data;
  int ret = MbedTLS_cmac_reset(cmac_instance);
  if (ret != CMAC_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CMAC reset failed");
    return;
  }
  SET_RETURN(*v);
}

static void
c_digest(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint8_t *cmac_instance = v->instance->data;
  unsigned char output[16];
  int ret;
  ret = MbedTLS_cmac_finish(cmac_instance, output);
  if (ret != CMAC_SUCCESS) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CMAC finish failed");
    return;
  }
  MbedTLS_cmac_free(cmac_instance);
  mrbc_value digest = mrbc_string_new(vm, output, sizeof(output));
  mrbc_incref(&v[0]);
  SET_RETURN(digest);
}

void
gem_mbedtls_cmac_init(mrbc_vm *vm, mrbc_class *module_MbedTLS)
{
  mrbc_class *class_MbedTLS_CMAC = mrbc_define_class_under(vm, module_MbedTLS, "CMAC", mrbc_class_object);

  mrbc_define_method(vm, class_MbedTLS_CMAC, "_init_aes", c__init_aes);
  mrbc_define_method(vm, class_MbedTLS_CMAC, "update", c_update);
  mrbc_define_method(vm, class_MbedTLS_CMAC, "reset", c_reset);
  mrbc_define_method(vm, class_MbedTLS_CMAC, "digest", c_digest);
}
