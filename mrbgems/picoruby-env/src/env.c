#include <stdlib.h>
#include "../include/env.h"

#if defined(PICORB_VM_MRUBY)

#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/hash.h>
#include <mruby/string.h>

static mrb_value
mrb_env_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value hash = mrb_hash_new(mrb);
  mrb_iv_set(mrb, self, MRB_IVSYM(env), hash);

  char *key, *value;
  while (1) {
    env_get_key_value(&key, &value);
    if (key == NULL) {
      break;
    }
    mrb_value key_value = mrb_str_new_cstr(mrb, key);
    mrb_value value_value = mrb_str_new_cstr(mrb, value);
    mrb_hash_set(mrb, hash, key_value, value_value);
  }

  return self;
}

static mrb_value
mrb_env_setenv(mrb_state *mrb, mrb_value self)
{
  const char *key;
  const char *value;
  mrb_get_args(mrb, "zz", &key, &value);
  env_setenv(key, value);
  return mrb_nil_value();
}

void
mrb_picoruby_env_gem_init(mrb_state* mrb)
{
  struct RClass *class_ENVClass = mrb_define_class_id(mrb, MRB_SYM(ENVClass), mrb->object_class);

  mrb_define_method_id(mrb, class_ENVClass, MRB_SYM(initialize), mrb_env_initialize, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_ENVClass, MRB_SYM(setenv), mrb_env_setenv, MRB_ARGS_REQ(2));
}

void
mrb_picoruby_env_gem_final(mrb_state* mrb)
{
}

#elif defined(PICORB_VM_MRUBYC)

#include <mrubyc.h>

static void
c_env_setenv(struct VM *vm, mrbc_value v[], int argc)
{
  const char *key = (const char *)GET_STRING_ARG(1);
  const char *value = (const char *)GET_STRING_ARG(2);
  setenv(key, value, 1);
  mrbc_value hash = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("env"));
  mrbc_value key_value = mrbc_string_new_cstr(vm, key);
  mrbc_value value_value = mrbc_string_new_cstr(vm, value);
  mrbc_hash_set(&hash, &key_value, &value_value);
  mrbc_incref(&key_value);
  mrbc_incref(&value_value);
  SET_RETURN(value_value);
}

static void
c_env_new(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value hash = mrbc_hash_new(vm, 0);
  mrbc_value self = mrbc_instance_new(vm, v->cls, 0);
  mrbc_instance_setiv(&self, mrbc_str_to_symid("env"), &hash);

  char *key, *value;
  while (1) {
    env_get_key_value(&key, &value);
    if (key == NULL) {
      break;
    }
    mrbc_value key_value = mrbc_string_new_cstr(vm, key);
    mrbc_value value_value = mrbc_string_new_cstr(vm, value);
    mrbc_hash_set(&hash, &key_value, &value_value);
  }

  SET_RETURN(self);
}

void
mrbc_env_init(mrbc_vm *vm)
{
  mrbc_class *class_ENVClass = mrbc_define_class(vm, "ENVClass", MRBC_CLASS(Object));

  mrbc_define_method(vm, class_ENVClass, "new", c_env_new);
  mrbc_define_method(vm, class_ENVClass, "setenv", c_env_setenv);
}

#endif
