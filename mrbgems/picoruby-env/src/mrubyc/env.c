#include <mrubyc.h>
#include <time.h>

typedef struct ENV {
  mrbc_value hash;
} ENV;

static void
c_env_aref(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type");
    return;
  }
  ENV *env = (ENV *)v->instance->data;
  mrbc_value key = GET_ARG(1);
  mrbc_value value = mrbc_hash_get(&env->hash, &key);
  mrbc_incref(&value);
  SET_RETURN(value);
}

static void
c_env_delete(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type");
    return;
  }
  ENV *env = (ENV *)v->instance->data;
  mrbc_value key = GET_ARG(1);
  mrbc_value value = mrbc_hash_remove(&env->hash, &key);
  mrbc_incref(&value);
  SET_RETURN(value);
  ENV_unsetenv((const char *)GET_STRING_ARG(1));
}

static void
c_env_aset(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (v[1].tt != MRBC_TT_STRING || v[2].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong type");
    return;
  }
  ENV *env = (ENV *)v->instance->data;
  mrbc_value key = GET_ARG(1);
  mrbc_value value = GET_ARG(2);
  mrbc_incref(&key);
  mrbc_incref(&value);
  mrbc_incref(&value);
  mrbc_hash_set(&env->hash, &key, &value);
  SET_RETURN(value);
  ENV_setenv((const char *)key.string->data, (const char *)value.string->data, 1);
  if (strcmp((const char *)key.string->data, "TZ") == 0) {
    tzset(); // Necessary for POSIX timezone change
  }
}

static void
c_env_new(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(ENV));
  ENV *env = (ENV *)self.instance->data;
  env->hash = mrbc_hash_new(vm, 5);

  char *key, *value;
  size_t key_len, value_len;
  while (1) {
    ENV_get_key_value(&key, &key_len, &value, &value_len);
    if (key == NULL) {
      break;
    }
    mrbc_value key_value = mrbc_string_new(vm, key, key_len);
    mrbc_value value_value = mrbc_string_new(vm, value, value_len);
    mrbc_incref(&key_value);
    mrbc_incref(&value_value);
    mrbc_hash_set(&env->hash, &key_value, &value_value);
  }

  SET_RETURN(self);
}

static void
c_env__hash(struct VM *vm, mrbc_value v[], int argc)
{
  ENV *env = (ENV *)v->instance->data;
  mrbc_incref(&env->hash);
  SET_RETURN(env->hash);
}


void
mrbc_env_init(mrbc_vm *vm)
{
  mrbc_class *class_ENVClass = mrbc_define_class(vm, "ENVClass", MRBC_CLASS(Object));

  mrbc_define_method(vm, class_ENVClass, "new", c_env_new);
  mrbc_define_method(vm, class_ENVClass, "[]=", c_env_aset);
  mrbc_define_method(vm, class_ENVClass, "[]", c_env_aref);
  mrbc_define_method(vm, class_ENVClass, "delete", c_env_delete);
  // private
  mrbc_define_method(vm, class_ENVClass, "_hash", c_env__hash);
}
