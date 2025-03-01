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
