#include <mrubyc.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

/*
 * picotest_doubles = [
 *   {
 *     type: any_instance_of,
 *     doubled_obj_id: doubled_obj_id,
 *     method_id: method_id,
 *     return_value: return_value,
 *     singleton_class_name: nil,
 *   },
 *   {
 *     type: stub,
 *     doubled_obj_id: doubled_obj_id,
 *     method_id: method_id,
 *     return_value: return_value,
 *     singleton_class_name: singleton_class_name,
 *   },
 *   {
 *     type: mock,
 *     expected_count: 1,
 *     actual_count: 0,
 *     doubled_obj_id: doubled_obj_id,
 *     method_id: method_id,
 *     return_value: return_value,
 *     singleton_class_name: singleton_class_name,
 *   }
 * ]
 */

static bool
search_return_value(struct VM *vm, intptr_t doubled_obj_id, mrbc_sym called_method_id, mrbc_value *return_value)
{
  mrbc_value *picotest_doubles = mrbc_get_global(mrbc_str_to_symid("$picotest_doubles"));
  for (int i = 0 ; i < picotest_doubles->array->n_stored; i++) {
    mrbc_value double_method = picotest_doubles->array->data[i];
    if (mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("doubled_obj_id"))).i == doubled_obj_id &&
        mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("method_id"))).sym_id == called_method_id) {
      *return_value = mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("return_value")));
      mrbc_incref(return_value);
      if (mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("type"))).sym_id == mrbc_str_to_symid("mock")) {
        mrbc_value actual_count = mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("actual_count")));
        actual_count.i++;
        mrbc_hash_set(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("actual_count")), &actual_count);
      }
      return true;
    }
  }
  return false;
}

static inline mrbc_sym
callee_symid(struct VM *vm)
{
  /*
   * mrbc_get_callee_name() in vm.c may not work correctly
   * in the future as it depends on the VM code spec.
   */
  return mrbc_get_callee_symid(vm);
}

static void
c__double_method(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_sym called_method_id = callee_symid(vm);
  mrbc_value return_value = mrbc_nil_value();

  mrbc_class *cls;
  switch (v[0].tt) {
    case MRBC_TT_NIL: cls = MRBC_CLASS(NilClass); break;
    case MRBC_TT_FALSE: cls = MRBC_CLASS(FalseClass); break;
    case MRBC_TT_TRUE: cls = MRBC_CLASS(TrueClass); break;
    case MRBC_TT_INTEGER: cls = MRBC_CLASS(Integer); break;
    case MRBC_TT_FLOAT: cls = MRBC_CLASS(Float); break;
    case MRBC_TT_SYMBOL: cls = MRBC_CLASS(Symbol); break;
    case MRBC_TT_CLASS:
    case MRBC_TT_MODULE: cls = v[0].cls; break;
    case MRBC_TT_OBJECT: cls = v[0].instance->cls; break;
    case MRBC_TT_PROC: cls = MRBC_CLASS(Proc); break;
    case MRBC_TT_ARRAY: cls = MRBC_CLASS(Array); break;
    case MRBC_TT_STRING: cls = MRBC_CLASS(String); break;
    case MRBC_TT_RANGE: cls = MRBC_CLASS(Range); break;
    case MRBC_TT_HASH: cls = MRBC_CLASS(Hash); break;
    case MRBC_TT_EXCEPTION: cls = MRBC_CLASS(Exception); break;
    default: {
      mrbc_raisef(vm, MRBC_CLASS(TypeError), "Invalid target object: %d", v[0].tt);
      return;
    }
  }

  if (!search_return_value(vm, (intptr_t)v[0].i, called_method_id, &return_value)) {
    mrbc_raisef(vm, MRBC_CLASS(NoMethodError), "undefined method `%s' for %s", mrbc_symid_to_str(called_method_id), mrbc_symid_to_str(cls->sym_id));
    return;
  }

  SET_RETURN(return_value);
}

static bool
search_return_value_any_instance_of(struct VM *vm, mrbc_class *cls, mrbc_sym called_method_id, mrbc_value *return_value)
{
  mrbc_value *picotest_doubles = mrbc_get_global(mrbc_str_to_symid("$picotest_doubles"));
  for (int i = 0 ; i < picotest_doubles->array->n_stored; i++) {
    mrbc_value double_method = picotest_doubles->array->data[i];
    if (mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("doubled_obj_id"))).i == (intptr_t)cls &&
        mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("method_id"))).sym_id == called_method_id) {
      *return_value = mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("return_value")));
      mrbc_incref(return_value);
      if (mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("type"))).sym_id == mrbc_str_to_symid("mock")) {
        mrbc_value actual_count = mrbc_hash_get(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("actual_count")));
        actual_count.i++;
        mrbc_hash_set(&double_method, &mrbc_symbol_value(mrbc_str_to_symid("actual_count")), &actual_count);
      }
      return true;
    }
  }
  return false;
}

static void
c__double_method_any_instance_of(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_sym called_method_id = callee_symid(vm);
  mrbc_value return_value = mrbc_nil_value();

  mrbc_class *cls;
  switch (v[0].tt) {
    case MRBC_TT_NIL: cls = MRBC_CLASS(NilClass); break;
    case MRBC_TT_FALSE: cls = MRBC_CLASS(FalseClass); break;
    case MRBC_TT_TRUE: cls = MRBC_CLASS(TrueClass); break;
    case MRBC_TT_INTEGER: cls = MRBC_CLASS(Integer); break;
    case MRBC_TT_FLOAT: cls = MRBC_CLASS(Float); break;
    case MRBC_TT_SYMBOL: cls = MRBC_CLASS(Symbol); break;
    case MRBC_TT_CLASS:
    case MRBC_TT_MODULE: cls = v[0].cls; break;
    case MRBC_TT_OBJECT: cls = v[0].instance->cls; break;
    case MRBC_TT_PROC: cls = MRBC_CLASS(Proc); break;
    case MRBC_TT_ARRAY: cls = MRBC_CLASS(Array); break;
    case MRBC_TT_STRING: cls = MRBC_CLASS(String); break;
    case MRBC_TT_RANGE: cls = MRBC_CLASS(Range); break;
    case MRBC_TT_HASH: cls = MRBC_CLASS(Hash); break;
    case MRBC_TT_EXCEPTION: cls = MRBC_CLASS(Exception); break;
    default: {
      mrbc_raisef(vm, MRBC_CLASS(TypeError), "Invalid target object: %d", v[0].tt);
      return;
    }
  }

  if (!search_return_value_any_instance_of(vm, cls, called_method_id, &return_value)) {
    mrbc_raisef(vm, MRBC_CLASS(NoMethodError), "undefined method `%s' for %s", mrbc_symid_to_str(called_method_id), mrbc_symid_to_str(cls->sym_id));
    return;
  }

  SET_RETURN(return_value);
}

static void
c_double_remove_singleton(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value doubled_obj = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("doubled_obj"));

 // if (MRBC_TT_OBJECT == doubled_obj.tt) {
  if (MRBC_TT_INC_DEC_THRESHOLD < doubled_obj.tt) {
    mrbc_value singleton_class_name = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("singleton_class_name"));
    if (singleton_class_name.tt == MRBC_TT_STRING) {
      mrbc_class *singleton_class = mrbc_get_class_by_name((const char *)singleton_class_name.string->data);
      mrbc_class **cls = &doubled_obj.instance->cls;
      while (*cls && *cls != singleton_class) {
        cls = &(*cls)->super;
      }
      if (*cls == singleton_class) {
        *cls = singleton_class->super;
      }
    }
  }
  else {
    mrbc_value method_id = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("method_id"));
    mrbc_class *cls;
    switch (doubled_obj.tt) {
      case MRBC_TT_CLASS:
      case MRBC_TT_MODULE:
        cls = doubled_obj.cls;
        break;
      case MRBC_TT_PROC:
        cls = MRBC_CLASS(Proc);
        break;
      case MRBC_TT_ARRAY:
        cls = MRBC_CLASS(Array);
        break;
      case MRBC_TT_STRING:
        cls = MRBC_CLASS(String);
        break;
      case MRBC_TT_RANGE:
        cls = MRBC_CLASS(Range);
        break;
      case MRBC_TT_HASH:
        cls = MRBC_CLASS(Hash);
        break;
      case MRBC_TT_EXCEPTION:
        cls = MRBC_CLASS(Exception);
        break;
      default:
        mrbc_raisef(vm, MRBC_CLASS(TypeError), "Invalid target object: %d", doubled_obj.tt);
        return;
    }
    mrbc_method **method = &cls->method_link;
    while ((*method)->sym_id != method_id.i) {
      method = &(*method)->next;
      if (!(*method)) goto not_found;
    }
    *method = (*method)->next;
  }
not_found:
  SET_NIL_RETURN();
}

static void
c_double__alloc(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_value self = mrbc_instance_new(vm, v->cls, 0);
  SET_RETURN(self);
}

static void
c_double_define_method_any_instance_of(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_class *cls;
  mrbc_value doubled_obj = v[2];
  cls = doubled_obj.cls;
  mrbc_value method_id = v[1];
  const char *method_name = mrbc_symid_to_str(method_id.sym_id);
  mrbc_define_method(vm, cls, method_name, c__double_method_any_instance_of);
  SET_NIL_RETURN();
}

static void
c_double_define_method(struct VM *vm, mrbc_value v[], int argc)
{
  mrbc_class *singleton_class;
  mrbc_class *super_class; // original class
  mrbc_value doubled_obj = v[2];
  switch (doubled_obj.tt) {
    case MRBC_TT_NIL:
    case MRBC_TT_FALSE:
    case MRBC_TT_TRUE:
    case MRBC_TT_INTEGER:
    case MRBC_TT_FLOAT:
    case MRBC_TT_SYMBOL:
      {
        mrbc_raisef(vm, MRBC_CLASS(TypeError), "Invalid target object: %d", v[2].tt);
        return;
      }
    case MRBC_TT_CLASS:
    case MRBC_TT_MODULE:
      super_class = doubled_obj.cls;
      break;
    case MRBC_TT_OBJECT:
      super_class = doubled_obj.instance->cls;
      break;
    case MRBC_TT_PROC:
      super_class = MRBC_CLASS(Proc);
      break;
    case MRBC_TT_ARRAY:
      super_class = MRBC_CLASS(Array);
      break;
    case MRBC_TT_STRING:
      super_class = MRBC_CLASS(String);
      break;
    case MRBC_TT_RANGE:
      super_class = MRBC_CLASS(Range);
      break;
    case MRBC_TT_HASH:
      super_class = MRBC_CLASS(Hash);
      break;
    case MRBC_TT_EXCEPTION:
      super_class = MRBC_CLASS(Exception);
      break;
    default:
      {
        mrbc_raisef(vm, MRBC_CLASS(TypeError), "Invalid target object: %d", v[2].tt);
        return;
      }
  }

  mrbc_value method_id = v[1];
  const char *method_name;
  if (method_id.tt == MRBC_TT_SYMBOL) {
    method_name = mrbc_symid_to_str(method_id.sym_id);
  } else if (method_id.tt == MRBC_TT_STRING) {
    method_name = (const char *)method_id.string->data;
  } else {
    mrbc_raisef(vm, MRBC_CLASS(TypeError), "method_id must be Symbol but %d", method_id.tt);
    return;
  }

  if (MRBC_TT_INC_DEC_THRESHOLD < doubled_obj.tt) {
    for (int i = 0; i < vm->regs_size; i++) {
      if (vm->regs[i].i == doubled_obj.i) {
        vm->regs[i].tt = MRBC_TT_OBJECT;
        break;
      }
    }
    char buf[30];
    sprintf(buf, "S_%p_%08" PRId64, doubled_obj.instance, (int64_t)method_id.sym_id);
    char *name = mrbc_alloc(vm, strlen(buf) + 1);
    strcpy(name, buf);
    name[strlen(buf)] = '\0';
    singleton_class = mrbc_define_class(vm, name, super_class);
    doubled_obj.instance->cls = singleton_class;
    doubled_obj.instance->ref_count++;
    mrbc_define_method(vm, singleton_class, method_name, c__double_method);
    mrbc_value singleton_class_name = mrbc_string_new(vm, name, strlen(name));
    SET_RETURN(singleton_class_name);
  }
  else {
    // modifying method_link instead of modifying class
    mrbc_define_method(vm, super_class, method_name, c__double_method);
    SET_NIL_RETURN();
  }
}

void
mrbc_picotest_init(mrbc_vm *vm)
{
  mrbc_class *module_Picotest = mrbc_define_module(vm, "Picotest");

  mrbc_class *class_Picotest_Double = mrbc_define_class_under(vm, module_Picotest, "Double", mrbc_class_object);
  mrbc_define_method(vm, class_Picotest_Double, "remove_singleton", c_double_remove_singleton);
  mrbc_define_method(vm, class_Picotest_Double, "_alloc", c_double__alloc);
  mrbc_define_method(vm, class_Picotest_Double, "define_method", c_double_define_method);
  mrbc_define_method(vm, class_Picotest_Double, "define_method_any_instance_of", c_double_define_method_any_instance_of);

}
