
#include "../../include/env.h"
extern char **environ;

void
c_env_new(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value hash = mrbc_hash_new(vm, 0);
  for (char **env = environ; *env != NULL; env++) {
    char *key = strtok(*env, "=");
    char *value = strtok(NULL, "=");
    mrbc_value key_value = mrbc_string_new_cstr(vm, key);
    mrbc_value value_value = mrbc_string_new_cstr(vm, value);
    mrbc_hash_set(&hash, &key_value, &value_value);
  }
  mrbc_value self = mrbc_instance_new(vm, v->cls, 0);
  mrbc_instance_setiv(&self, mrbc_str_to_symid("env"), &hash);
  SET_RETURN(self);
}

