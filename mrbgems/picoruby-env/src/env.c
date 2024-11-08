#include <stdlib.h>

#include "../include/env.h"

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

void
mrbc_env_init(mrbc_vm *vm)
{
  mrbc_class *class_ENVClass = mrbc_define_class(vm, "ENVClass", MRBC_CLASS(Object));

  mrbc_define_method(vm, class_ENVClass, "new", c_env_new);
  mrbc_define_method(vm, class_ENVClass, "setenv", c_env_setenv);
}
