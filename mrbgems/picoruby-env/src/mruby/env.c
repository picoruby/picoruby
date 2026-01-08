#include <time.h>
#include <string.h>

#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include "mruby/class.h"
#include "mruby/data.h"

typedef struct ENV {
  mrb_value hash;
} ENV;

static void
mrb_env_free(mrb_state *mrb, void *ptr) {
  ENV *env = (ENV *)ptr;
  mrb_value hash = env->hash;
  if (mrb_hash_p(hash)) {
    mrb_hash_clear(mrb, hash);
  }
  mrb_gc_unregister(mrb, hash);
  mrb_free(mrb, ptr);
}

struct mrb_data_type mrb_env_type = {
  "ENV", mrb_env_free
};


static mrb_value
mrb_env_s_new(mrb_state *mrb, mrb_value klass)
{
  ENV *env = (ENV *)mrb_malloc(mrb, sizeof(ENV));
  mrb_value self = mrb_obj_value(Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_env_type, env));
  env->hash = mrb_hash_new_capa(mrb, 5);
  mrb_gc_register(mrb, env->hash);

  char *key, *value;
  size_t key_len, value_len;
  int ai = mrb_gc_arena_save(mrb);
  while (1) {
    ENV_get_key_value(&key, &key_len, &value, &value_len);
    if (key == NULL) {
      break;
    }
    mrb_value key_value = mrb_str_new(mrb, key, key_len);
    mrb_value value_value = mrb_str_new(mrb, value, value_len);
    mrb_hash_set(mrb, env->hash, key_value, value_value);
  }
  mrb_gc_arena_restore(mrb, ai);

  return self;
}

static mrb_value
mrb_env_aset(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "SS", &key, &value);
  ENV *env = (ENV *)mrb_data_get_ptr(mrb, self, &mrb_env_type);
  mrb_hash_set(mrb, env->hash, key, value);
  ENV_setenv(RSTRING_PTR(key), RSTRING_PTR(value), 1);
  if (strcmp((const char *)RSTRING_PTR(key), "TZ") == 0) {
    tzset(); // Necessary for POSIX timezone change
  }
  return value;
}

static mrb_value
mrb_env_aref(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "S", &key);
  ENV *env = (ENV *)mrb_data_get_ptr(mrb, self, &mrb_env_type);
  value = mrb_hash_fetch(mrb, env->hash, key, mrb_nil_value());
  return value;
}

static mrb_value
mrb_env_delete(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "S", &key);
  ENV *env = (ENV *)mrb_data_get_ptr(mrb, self, &mrb_env_type);
  value = mrb_hash_delete_key(mrb, env->hash, key);
  ENV_unsetenv(RSTRING_PTR(key));
  return value;
}

static mrb_value
mrb_env__hash(mrb_state *mrb, mrb_value self)
{
  ENV *env = (ENV *)mrb_data_get_ptr(mrb, self, &mrb_env_type);
  return env->hash;
}

void
mrb_picoruby_env_gem_init(mrb_state* mrb)
{
  struct RClass *class_ENVClass = mrb_define_class_id(mrb, MRB_SYM(ENVClass), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_ENVClass, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_ENVClass, MRB_SYM(new), mrb_env_s_new, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_ENVClass, MRB_OPSYM(aset), mrb_env_aset, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_ENVClass, MRB_OPSYM(aref), mrb_env_aref, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_ENVClass, MRB_SYM(delete), mrb_env_delete, MRB_ARGS_REQ(1));
  mrb_define_private_method_id(mrb, class_ENVClass, MRB_SYM(_hash), mrb_env__hash, MRB_ARGS_NONE());
}

void
mrb_picoruby_env_gem_final(mrb_state* mrb)
{
}
