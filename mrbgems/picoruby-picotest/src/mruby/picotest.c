#include "mruby.h"
#include "mruby/class.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/data.h"
#include "mruby/variable.h"
#include "mruby/hash.h"
#include "mruby/array.h"
#include "mruby/presym.h"

// This function will be the body of all stubbed methods.
static mrb_value
mruby_method_missing_for_double(mrb_state *mrb, mrb_value self)
{
  mrb_sym mid = mrb->c->ci->mid;

  // Get the global doubles array
  mrb_value global_doubles = mrb_gv_get(mrb, mrb_intern_lit(mrb, "$picotest_doubles"));
  if (mrb_nil_p(global_doubles)) {
    return mrb_nil_value(); // Should not happen
  }

  // Find the correct double_data for this object and method
  mrb_int len = RARRAY_LEN(global_doubles);
  for (mrb_int i = 0; i < len; i++) {
    mrb_value double_data_val = mrb_ary_ref(mrb, global_doubles, i);
    if (mrb_hash_p(double_data_val)) {
      mrb_value doubled_obj_id_val = mrb_hash_get(mrb, double_data_val, mrb_symbol_value(MRB_SYM(doubled_obj_id)));
      mrb_value method_id_val = mrb_hash_get(mrb, double_data_val, mrb_symbol_value(MRB_SYM(method_id)));

      if (!mrb_nil_p(doubled_obj_id_val) && !mrb_nil_p(method_id_val)) {
        if (mrb_obj_id(self) == mrb_integer(doubled_obj_id_val) && mrb_symbol(method_id_val) == mid) {
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
picotest_double_define_method(mrb_state *mrb, mrb_value self)
{
  mrb_value method_name_val, obj;
  mrb_get_args(mrb, "So", &method_name_val, &obj);
  mrb_sym method_name = mrb_symbol(method_name_val);
  mrb_define_singleton_method(mrb, mrb_obj_ptr(obj), mrb_sym_name(mrb, method_name), mruby_method_missing_for_double, MRB_ARGS_ANY());
  return mrb_nil_value();
}

static mrb_value
picotest_double_define_method_any_instance_of(mrb_state *mrb, mrb_value self)
{
  mrb_value method_name_val, klass_val;
  mrb_get_args(mrb, "So", &method_name_val, &klass_val);
  mrb_sym method_name = mrb_symbol(method_name_val);
  struct RClass *klass = mrb_class_ptr(klass_val);
  mrb_define_method(mrb, klass, mrb_sym_name(mrb, method_name), mruby_method_missing_for_double, MRB_ARGS_ANY());
  return mrb_nil_value();
}

static mrb_value
picotest_double_remove_singleton(mrb_state *mrb, mrb_value self)
{
  return mrb_nil_value(); // No-op
}

void
mrb_picoruby_picotest_gem_init(mrb_state* mrb)
{
  struct RClass *picotest_module = mrb_define_module_id(mrb, MRB_SYM(Picotest));
  struct RClass *double_class = mrb_define_class_under_id(mrb, picotest_module, MRB_SYM(Double), mrb->object_class);

  mrb_define_method_id(mrb, double_class, MRB_SYM(define_method), picotest_double_define_method, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, double_class, MRB_SYM(define_method_any_instance_of), picotest_double_define_method_any_instance_of, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, double_class, MRB_SYM(remove_singleton), picotest_double_remove_singleton, MRB_ARGS_NONE());

  // Initialize the global array to store double data
  mrb_gv_set(mrb, MRB_GVSYM(picotest_doubles), mrb_ary_new(mrb));
}

void
mrb_picoruby_picotest_gem_final(mrb_state* mrb)
{
}
