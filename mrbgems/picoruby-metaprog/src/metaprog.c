#include <mrc_common.h>
#include <mrc_ccontext.h>
#include <mrc_compile.h>
#include <mrc_dump.h>
#include <mrubyc.h>
#include <stdbool.h>
#include <unistd.h>

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

#if !defined(MRBC_DEBUG)
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
#endif

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
      sym_id = v[1].sym_id;
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
      sym_id = v[1].sym_id;
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

static void
c_object_instance_of_q(mrbc_vm *vm, mrbc_value *v, int argc)
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

static void
c_object_const_get(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value *value;
  mrbc_sym sym_id;
  if (v[1].tt == MRBC_TT_SYMBOL) {
    sym_id = v[1].sym_id;
  } else if (v[1].tt == MRBC_TT_STRING) {
    sym_id = mrbc_str_to_symid((const char *)GET_STRING_ARG(1));
  } else {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "not a symbol nor a string");
    return;
  }
  value = mrbc_get_const(sym_id);
  if (!value) {
    mrbc_raisef(vm, MRBC_CLASS(NameError), "uninitialized constant %s", mrbc_symid_to_str(sym_id));
    return;
  }
  mrbc_incref(value);
  SET_RETURN(*value);
}

static void
c_object_class_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[0].tt == MRBC_TT_CLASS) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_object_ancestors(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[0].tt != MRBC_TT_CLASS && v[0].tt != MRBC_TT_MODULE) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method `ancestors'");
    return;
  }
  mrbc_value *ancestor;
  mrbc_value ancestors = mrbc_array_new(vm, 0);
  mrbc_class *cls = find_class_by_object(&v[0]);
  while (cls) {
    ancestor = mrbc_get_const(cls->sym_id);
    mrbc_array_push(&ancestors, ancestor);
    cls = cls->super;
  }
  SET_RETURN(ancestors);
}

static void
c_proc__get_self(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[0].tt != MRBC_TT_PROC) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method `get_self'");
  } else {
    SET_RETURN(v[0].proc->self);
  }
}

static void
c_proc__set_self(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[0].tt != MRBC_TT_PROC) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method `set_self'");
  } else {
    mrbc_incref(&v[0]);
    mrbc_incref(&v[1]);
    v[0].proc->self = v[1];
    SET_RETURN(v[1]);
  }
}

#define MAX_CALLINFO 100

static void
c_kernel_caller(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int start = 1;
  int length = MAX_CALLINFO;
  mrbc_value ary = mrbc_array_new(vm, 0);
  mrbc_callinfo *ci;
  if (2 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (0 < argc) {
    start = GET_INT_ARG(1);
  }
  if (1 < argc) {
    length = GET_INT_ARG(2);
    if (length < 0) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "negative length");
      return;
    }
  }
  ci = vm->callinfo_tail; // this does not include `caller' itself
  while (1) {
    if (start <= 0) {
      if (length <= 0) {
        break;
      }
      length--;
      mrbc_value str = mrbc_string_new_cstr(vm, mrbc_symid_to_str(ci->method_id));
      mrbc_array_push(&ary, &str);
    }
    start--;
    ci = ci->prev;
    if (ci == NULL && 0 < length) {
      mrbc_value str = mrbc_string_new_cstr(vm, "<main>");
      mrbc_array_push(&ary, &str);
      break;
    }
  }
  SET_RETURN(ary);
}

static void
c_kernel_eval(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char *utf8 = (char *)GET_STRING_ARG(1);
  mrc_ccontext *c = mrc_ccontext_new(NULL);
  mrc_irep *irep = mrc_load_string_cxt(c, (const uint8_t **)&utf8, strlen(utf8));
  int result;
  uint8_t *mrb = NULL;
  size_t mrb_size = 0;
  result = mrc_dump_irep(c, irep, 0, &mrb, &mrb_size);
  if (result != MRC_DUMP_OK) {
    mrbc_raise(vm, MRBC_CLASS(Exception), "syntax error found");
    return;
  }
  mrc_irep_free(c, irep);
  mrc_ccontext_free(c);
  mrbc_tcb *tcb = mrbc_create_task(mrb, NULL);
  tcb->vm.flag_preemption = 0;
  SET_NIL_RETURN();
}

#define METAPROG_PATH_MAX 1024

static void
c_rbconfig_ruby(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char *picoruby_path = NULL;
#ifdef _WIN32
  char path[METAPROG_PATH_MAX];
  if (GetModuleFileName(NULL, path, METAPROG_PATH_MAX) != 0) {
    picoruby_path = _strdup(path);
  }
#elif defined(__linux__)
  char path[METAPROG_PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len != -1) {
    path[len] = '\0';
    picoruby_path = strdup(path);
  }
#elif defined(__APPLE__)
  char path[METAPROG_PATH_MAX];
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) == 0) {
    char real_path[METAPROG_PATH_MAX];
    if (realpath(path, real_path) != NULL) {
      picoruby_path = strdup(real_path);
    }
  }
#else
  // maybe baremetal
  SET_NIL_RETURN();
  return;
#endif
  mrbc_value path_str = mrbc_string_new_cstr(vm, picoruby_path);
  SET_RETURN(path_str);
}

void
mrbc_metaprog_init(mrbc_vm *vm)
{
  mrbc_define_method(vm, mrbc_class_object, "send", c_object_send);
  mrbc_define_method(vm, mrbc_class_object, "methods", c_object_methods);
  mrbc_define_method(vm, mrbc_class_object, "__id__", c_object_id);
#if !defined(MRBC_DEBUG)
  mrbc_define_method(vm, mrbc_class_object, "object_id", c_object_id);
  mrbc_define_method(vm, mrbc_class_object, "instance_variables", c_object_instance_variables);
#endif
  mrbc_define_method(vm, mrbc_class_object, "instance_variable_get", c_object_instance_variable_get);
  mrbc_define_method(vm, mrbc_class_object, "instance_variable_set", c_object_instance_variable_set);
  mrbc_define_method(vm, mrbc_class_object, "instance_of?", c_object_instance_of_q);
  mrbc_define_method(vm, mrbc_class_object, "respond_to?", c_object_respond_to_q);
  mrbc_define_method(vm, mrbc_class_object, "const_get", c_object_const_get);
  mrbc_define_method(vm, mrbc_class_object, "class?", c_object_class_q);
  mrbc_define_method(vm, mrbc_class_object, "ancestors", c_object_ancestors);

  mrbc_define_method(vm, mrbc_class_object, "_get_self", c_proc__get_self);
  mrbc_define_method(vm, mrbc_class_object, "_set_self", c_proc__set_self);

  mrbc_class *module_Kernel = mrbc_get_class_by_name("Kernel");
  mrbc_define_method(vm, module_Kernel, "caller", c_kernel_caller);
  mrbc_define_method(vm, module_Kernel, "eval", c_kernel_eval);

  mrbc_class *module_RbConfig = mrbc_define_module(vm, "RbConfig");
  mrbc_define_method(vm, module_RbConfig, "ruby", c_rbconfig_ruby);
}
