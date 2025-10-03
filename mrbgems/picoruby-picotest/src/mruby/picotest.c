#include "mruby.h"
#include "mruby/class.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/data.h"
#include "mruby/variable.h"
#include "mruby/hash.h"
#include "mruby/array.h"
#include "mruby/presym.h"

#include <stdbool.h>

// This function will be the body of all stubbed methods.
static mrb_value
mruby_method_missing_for_double(mrb_state *mrb, mrb_value self)
{
  mrb_sym mid = mrb->c->ci->mid;

  // Get the global doubles array
  mrb_value picotest_doubles = mrb_gv_get(mrb, MRB_GVSYM(picotest_doubles));
  if (mrb_nil_p(picotest_doubles)) {
    return mrb_nil_value(); // Should not happen
  }

  struct RClass *self_class = mrb_class(mrb, self);

  // Find the correct double_data for this object and method
  mrb_int len = RARRAY_LEN(picotest_doubles);
  for (mrb_int i = 0; i < len; i++) {
    mrb_value double_data_val = mrb_ary_ref(mrb, picotest_doubles, i);
    if (mrb_hash_p(double_data_val)) {
      mrb_value doubled_obj_id_val = mrb_hash_get(mrb, double_data_val, mrb_symbol_value(MRB_SYM(doubled_obj_id)));
      mrb_value method_id_val = mrb_hash_get(mrb, double_data_val, mrb_symbol_value(MRB_SYM(method_id)));
      mrb_value any_instance_of_val = mrb_hash_get(mrb, double_data_val, mrb_symbol_value(MRB_SYM(any_instance_of)));

      if (!mrb_nil_p(doubled_obj_id_val) && !mrb_nil_p(method_id_val)) {
        bool match = false;
        if (mrb_test(any_instance_of_val)) {
          // For any_instance_of, compare class pointer
          if ((mrb_int)self_class == mrb_integer(doubled_obj_id_val) && mrb_symbol(method_id_val) == mid) {
            match = true;
          }
        } else {
          // For regular stub/mock, compare object_id
          if (mrb_obj_id(self) == mrb_integer(doubled_obj_id_val) && mrb_symbol(method_id_val) == mid) {
            match = true;
          }
        }

        if (match) {
          // Found the data, return the value
          mrb_value return_value = mrb_hash_get(mrb, double_data_val, mrb_symbol_value(MRB_SYM(return_value)));
          // Increment actual_count if it's a mock
          mrb_value type = mrb_hash_get(mrb, double_data_val, mrb_symbol_value(MRB_SYM(type)));
          if (mrb_symbol(type) == MRB_SYM(mock)) {
            mrb_value actual_count = mrb_hash_get(mrb, double_data_val, mrb_symbol_value(MRB_SYM(actual_count)));
            mrb_hash_set(mrb, double_data_val, mrb_symbol_value(MRB_SYM(actual_count)), mrb_int_value(mrb, mrb_integer(actual_count) + 1));
          }
          return return_value;
        }
      }
    }
  }

  return mrb_nil_value(); // Should not be reached if logic is correct
}
static mrb_value
picotest_double_s__alloc(mrb_state *mrb, mrb_value klass)
{
  mrb_value doubed_obj;
  mrb_get_args(mrb, "o", &doubed_obj);
  struct RClass *klass_ptr = mrb_class_ptr(klass);
  return mrb_obj_new(mrb, klass_ptr, 0, NULL);
}

static mrb_value
picotest_double_define_method(mrb_state *mrb, mrb_value self)
{
  mrb_value method_name_val, doubled_obj;
  mrb_get_args(mrb, "oo", &method_name_val, &doubled_obj);
  mrb_sym method_name = mrb_symbol(method_name_val);

  struct RClass *singleton_class;
  struct RClass *super_class;

  enum mrb_vtype tt = mrb_type(doubled_obj);
  switch (tt) {
    case MRB_TT_FALSE:
    case MRB_TT_TRUE:
    case MRB_TT_INTEGER:
    case MRB_TT_FLOAT:
    case MRB_TT_SYMBOL:
      mrb_raisef(mrb, E_TYPE_ERROR, "Invalid target object: %d", tt);
      return mrb_nil_value();
    case MRB_TT_CLASS:
    case MRB_TT_MODULE:
    case MRB_TT_SCLASS:
      super_class = mrb_class_ptr(doubled_obj);
      break;
    case MRB_TT_OBJECT:
    case MRB_TT_PROC:
    case MRB_TT_ARRAY:
    case MRB_TT_STRING:
    case MRB_TT_RANGE:
    case MRB_TT_HASH:
    case MRB_TT_EXCEPTION:
      super_class = mrb_class(mrb, doubled_obj);
      break;
    default:
      mrb_raisef(mrb, E_TYPE_ERROR, "Invalid target object: %d", tt);
      return mrb_nil_value();
  }

  if (tt == MRB_TT_OBJECT || tt == MRB_TT_PROC || tt == MRB_TT_ARRAY ||
      tt == MRB_TT_STRING || tt == MRB_TT_RANGE || tt == MRB_TT_HASH ||
      tt == MRB_TT_EXCEPTION) {
    char buf[64];
    snprintf(buf, sizeof(buf), "S_%p_%d", mrb_ptr(doubled_obj), method_name);
    singleton_class = mrb_define_class(mrb, buf, super_class);
    mrb_basic_ptr(doubled_obj)->c = singleton_class;
    mrb_define_method_id(mrb, singleton_class, method_name, mruby_method_missing_for_double, MRB_ARGS_ANY());
    return mrb_str_new_cstr(mrb, buf);
  } else {
    mrb_define_method_id(mrb, super_class, method_name, mruby_method_missing_for_double, MRB_ARGS_ANY());
    return mrb_nil_value();
  }
}

static mrb_value
picotest_double_define_method_any_instance_of(mrb_state *mrb, mrb_value self)
{
  mrb_value method_name_val, klass_val;
  mrb_get_args(mrb, "oo", &method_name_val, &klass_val);
  mrb_sym method_name = mrb_symbol(method_name_val);
  struct RClass *klass = mrb_class_ptr(klass_val);
  mrb_define_method_id(mrb, klass, method_name, mruby_method_missing_for_double, MRB_ARGS_ANY());
  return mrb_nil_value();
}

static mrb_value
picotest_double_remove_singleton(mrb_state *mrb, mrb_value self)
{
  mrb_value doubled_obj = mrb_iv_get(mrb, self, MRB_SYM(doubled_obj));

  enum mrb_vtype tt = mrb_type(doubled_obj);
  if (tt == MRB_TT_OBJECT || tt == MRB_TT_PROC || tt == MRB_TT_ARRAY ||
      tt == MRB_TT_STRING || tt == MRB_TT_RANGE || tt == MRB_TT_HASH ||
      tt == MRB_TT_EXCEPTION) {
    mrb_value singleton_class_name = mrb_iv_get(mrb, self, MRB_SYM(singleton_class_name));
    if (mrb_string_p(singleton_class_name)) {
      struct RClass *singleton_class = mrb_class_get(mrb, mrb_string_cstr(mrb, singleton_class_name));
      struct RClass **cls = &mrb_basic_ptr(doubled_obj)->c;
      while (*cls && *cls != singleton_class) {
        cls = &(*cls)->super;
      }
      if (*cls == singleton_class) {
        *cls = singleton_class->super;
      }
    }
  } else if (tt == MRB_TT_CLASS || tt == MRB_TT_MODULE || tt == MRB_TT_SCLASS) {
    mrb_value method_id = mrb_iv_get(mrb, self, MRB_SYM(method_id));
    struct RClass *klass = mrb_class_ptr(doubled_obj);
    mrb_undef_method_id(mrb, klass, mrb_symbol(method_id));
  }
  return mrb_nil_value();
}

void
mrb_picoruby_picotest_gem_init(mrb_state* mrb)
{
  struct RClass *picotest_module = mrb_define_module_id(mrb, MRB_SYM(Picotest));
  struct RClass *double_class = mrb_define_class_under_id(mrb, picotest_module, MRB_SYM(Double), mrb->object_class);

  mrb_define_class_method_id(mrb, double_class, MRB_SYM(_alloc), picotest_double_s__alloc, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, double_class, MRB_SYM(define_method), picotest_double_define_method, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, double_class, MRB_SYM(define_method_any_instance_of), picotest_double_define_method_any_instance_of, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, double_class, MRB_SYM(remove_singleton), picotest_double_remove_singleton, MRB_ARGS_NONE());
}

void
mrb_picoruby_picotest_gem_final(mrb_state* mrb)
{
}
