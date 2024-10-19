#include <mrubyc.h>

/*
 * Restriction: Methods written in Ruby can not be called by send.
 */
static void
c_object_send(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_method method;
  const char *method_name;
  if (v[1].tt == MRBC_TT_SYMBOL) {
    method_name = (const char *)mrbc_symid_to_str(v[1].i);
  } else if (v[1].tt == MRBC_TT_STRING) {
    method_name = (const char *)GET_STRING_ARG(1);
  } else {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "Method name must be a string or symbol");
    return;
  }
  if (mrbc_find_method(&method, find_class_by_object(&v[0]), mrbc_str_to_symid(method_name)) == 0) {
    mrbc_raisef(vm, MRBC_CLASS(NoMethodError), "undefined method '%s'", method_name);
    return;
  }
  if( !method.c_func ) {
    mrbc_raisef(vm, MRBC_CLASS(RuntimeError), "C function is not defined for method '%s'", method_name);
    return;
  }

  mrbc_value send_v[argc];
  send_v[0] = v[0];
  for (int i = 1; i < argc; i++) {
    send_v[i] = v[i + 1];
  }

  method.func(vm, send_v, argc - 1);
  SET_RETURN(send_v[0]);
}

static void
c_object_methods(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value object = v[0];
  mrbc_value methods = mrbc_array_new(vm, 0);
  while (1) {
    mrbc_class *cls = NULL;
    switch (object.tt) {
      case MRBC_TT_NIL:       cls = MRBC_CLASS(NilClass);   break;
      case MRBC_TT_FALSE:     cls = MRBC_CLASS(FalseClass); break;
      case MRBC_TT_TRUE:      cls = MRBC_CLASS(TrueClass);  break;
      case MRBC_TT_INTEGER:   cls = MRBC_CLASS(Integer);    break;
      case MRBC_TT_FLOAT:     cls = MRBC_CLASS(Float);      break;
      case MRBC_TT_SYMBOL:    cls = MRBC_CLASS(Symbol);     break;
      case MRBC_TT_CLASS:     cls = object.cls;             break;
      case MRBC_TT_MODULE:    cls = object.cls;             break;
      case MRBC_TT_OBJECT:    cls = object.instance->cls;   break;
      case MRBC_TT_PROC:      cls = MRBC_CLASS(Proc);       break;
      case MRBC_TT_ARRAY:     cls = MRBC_CLASS(Array);      break;
      case MRBC_TT_STRING:    cls = MRBC_CLASS(String);     break;
      case MRBC_TT_RANGE:     cls = MRBC_CLASS(Range);      break;
      case MRBC_TT_HASH:      cls = MRBC_CLASS(Hash);       break;
      case MRBC_TT_EXCEPTION: cls = MRBC_CLASS(Exception);  break;
      default:
        mrbc_raisef(vm, MRBC_CLASS(Exception), "Unknown vtype: %d", object.tt);
        return;
    }
    mrbc_method *method;
    if (cls) {
      for( method = cls->method_link; method != 0; method = method->next ) {
        mrbc_array_push(&methods, &mrbc_symbol_value(method->sym_id));
      }
      if (0 < cls->num_builtin_method) {
        for (int i = 0; i < cls->num_builtin_method; i++) {
          mrbc_array_push(&methods, &mrbc_symbol_value(((struct RBuiltinClass *)cls)->method_symbols[i]));
        }
      }
    }
    if (!cls || cls->super == NULL) {
      break;
    }
    object = (mrbc_value){ .tt = MRBC_TT_CLASS, .cls = (void *)cls->super };
  }
  SET_RETURN(methods);
}

static void
c_object_instance_variables(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value ivars = mrbc_array_new(vm, 0);
  if( v[0].tt == MRBC_TT_OBJECT ) {
    mrbc_kv_handle *kvh = &v[0].instance->ivar;
    for( int i = 0; i < kvh->n_stored; i++ ) {
      const char *name = mrbc_symid_to_str(kvh->data[i].sym_id);
      char *buf = (char *)mrbc_raw_alloc_no_free(strlen(name) + 2);
      buf[0] = '@';
      strcpy(buf + 1, name);
      buf[strlen(name) + 1] = '\0';
      mrbc_array_push( &ivars, &mrbc_symbol_value(mrbc_str_to_symid(buf)) );
    }
  }

  SET_RETURN(ivars);
}

static void
c_object_instance_variable_get(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if( v[0].tt != MRBC_TT_OBJECT ) {
    SET_NIL_RETURN();
    return;
  }
  const char *name;
  mrbc_sym sym_id;
  switch (v[1].tt) {
    case MRBC_TT_SYMBOL:
      sym_id = v[1].i;
      name = mrbc_symid_to_str(sym_id);
      break;
    case MRBC_TT_STRING:
      name = (const char *)GET_STRING_ARG(1);
      break;
    default:
      mrbc_raise(vm, MRBC_CLASS(TypeError), "it is not a symbol nor a string");
      return;
  }
  if (name[0] != '@') {
    mrbc_raisef(vm, MRBC_CLASS(NameError), "'%s' is not allowed as an instance variable name", name);
    return;
  }
  mrbc_value val = mrbc_instance_getiv(&v[0], mrbc_str_to_symid(name + 1));
  if( val.tt == MRBC_TT_NIL ) {
    SET_NIL_RETURN();
  } else {
    SET_RETURN(val);
  }
}

static void
c_object_instance_variable_set(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if( v[0].tt != MRBC_TT_OBJECT ) {
    goto RETURN;
    return;
  }
  const char *name;
  mrbc_sym sym_id;
  switch (v[1].tt) {
    case MRBC_TT_SYMBOL:
      sym_id = v[1].i;
      name = mrbc_symid_to_str(sym_id);
      break;
    case MRBC_TT_STRING:
      name = (const char *)GET_STRING_ARG(1);
      break;
    default:
      mrbc_raise(vm, MRBC_CLASS(TypeError), "it is not a symbol nor a string");
      return;
  }
  if (name[0] != '@') {
    mrbc_raisef(vm, MRBC_CLASS(NameError), "'%s' is not allowed as an instance variable name", name);
    return;
  }
  mrbc_instance_setiv(&v[0], mrbc_str_to_symid(name + 1), &v[2]);
RETURN:
  SET_RETURN(v[2]);
}

static void
c_object_respond_to_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[1].tt != MRBC_TT_SYMBOL && v[1].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "not a symbol nor a string");
    return;
  }
  mrbc_method method;
  const char *method_name;
  if (v[1].tt == MRBC_TT_SYMBOL) {
    method_name = (const char *)mrbc_symid_to_str(v[1].i);
  } else {
    method_name = (const char *)GET_STRING_ARG(1);
  }
  if (mrbc_find_method(&method, find_class_by_object(&v[0]), mrbc_str_to_symid(method_name)) == 0) {
    SET_FALSE_RETURN();
  } else {
    SET_TRUE_RETURN();
  }
}

static void
c_object_id(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_int_t id = (mrbc_int_t)v[0].i;
  SET_INT_RETURN(id);
}

<<<<<<< HEAD
static void
c_instance_of_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (v[1].tt != MRBC_TT_CLASS) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "class or module required");
    return;
  }
  bool result = false;
  switch (v[0].tt) {
    case MRBC_TT_NIL: {
      if (v[1].cls == MRBC_CLASS(NilClass)) result = true;
      break;
    }
    case MRBC_TT_TRUE: {
      if (v[1].cls == MRBC_CLASS(TrueClass)) result = true;
      break;
    }
    case MRBC_TT_FALSE: {
      if (v[1].cls == MRBC_CLASS(FalseClass)) result = true;
      break;
    }
    case MRBC_TT_OBJECT: {
      mrbc_class *self_cls = find_class_by_object(&v[0]);
      if (self_cls == v[1].cls) result = true;
      break;
    }
    default:
      break;
  }
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

void
mrbc_metaprog_init(mrbc_vm *vm)
{
  mrbc_define_method(vm, mrbc_class_object, "send", c_object_send);
  mrbc_define_method(vm, mrbc_class_object, "methods", c_object_methods);
  mrbc_define_method(vm, mrbc_class_object, "instance_variables", c_object_instance_variables);
  mrbc_define_method(vm, mrbc_class_object, "instance_variable_get", c_object_instance_variable_get);
  mrbc_define_method(vm, mrbc_class_object, "instance_variable_set", c_object_instance_variable_set);
  mrbc_define_method(vm, mrbc_class_object, "respond_to?", c_object_respond_to_q);
  mrbc_define_method(vm, mrbc_class_object, "__id__", c_object_id);
  mrbc_define_method(vm, mrbc_class_object, "object_id", c_object_id);
}
