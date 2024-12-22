#include "../../include/env.h"
#include <stdio.h>

static const struct {
    const char *key;
    const char *value;
} env_table[] = {
    { "PWD", "" },      // Should be set in Shell.setup_system_files
    { "TERM", "ansi" }, // may be overwritten by IO.wait_terminal
    { "TZ", "JST-9" },
    { "WIFI_MODULE", "" },
    { NULL, NULL }
};

void c_env_new(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value hash = mrbc_hash_new(vm, 0);
  mrbc_value self = mrbc_instance_new(vm, v->cls, 0);
  mrbc_instance_setiv(&self, mrbc_str_to_symid("env"), &hash);

  for (int i = 0; env_table[i].key != NULL; i++) {
    mrbc_value key_value = mrbc_string_new_cstr(vm, env_table[i].key);
    mrbc_value value_value = mrbc_string_new_cstr(vm, env_table[i].value);
    mrbc_hash_set(&hash, &key_value, &value_value);
  }

  SET_RETURN(self);
}
