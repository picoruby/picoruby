#include <mrubyc.h>

typedef struct DataSubclass {
  mrbc_value member_keys;
} data_subclass_t;

typedef struct DataInstance {
  mrbc_value definition;
  mrbc_value members;
} data_instance_t;

static mrbc_class *class_Data;
/*
 * All instances of all definitions share this class. mruby/c cannot release
 * a class, so a class per definition would stay after the definition.
 * An instance holds its definition instead.
 */
static mrbc_class *class_Data_Instance;

/*
 * A definition is the object that Data.define returns: an instance of Data.
 * The methods of Data are also found on the class Data itself and on the
 * instances that a definition makes, so each method checks its receiver.
 */
static int
is_definition(const mrbc_value *v)
{
  return v->tt == MRBC_TT_OBJECT && v->instance->cls == class_Data;
}

/* ---- destructors ---- */

static void
data_subclass_destructor(mrbc_value *self)
{
  data_subclass_t *data = (data_subclass_t *)self->instance->data;
  mrbc_decref(&data->member_keys);
}

static void
data_instance_destructor(mrbc_value *self)
{
  data_instance_t *data = (data_instance_t *)self->instance->data;
  mrbc_decref(&data->members);
  mrbc_decref(&data->definition);
}

/*
 * Instance methods
 */

static void
c_method_missing(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value key = GET_ARG(1);
  data_instance_t *instance_data = (data_instance_t *)v->instance->data;
  mrbc_value members = instance_data->members;
  if (!mrbc_hash_search(&members, &key)) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "no such member");
    return;
  }
  mrbc_value value = mrbc_hash_get(&members, &key);
  mrbc_incref(&value);
  SET_RETURN(value);
}

static void
c_instance_to_h(mrbc_vm *vm, mrbc_value *v, int argc)
{
  data_instance_t *instance_data = (data_instance_t *)v->instance->data;
  /* Return a copy so that the caller cannot change the members. */
  mrbc_value members = mrbc_hash_dup(vm, &instance_data->members);
  SET_RETURN(members);
}

static void
c_instance_members(mrbc_vm *vm, mrbc_value *v, int argc)
{
  data_instance_t *instance_data = (data_instance_t *)v->instance->data;
  mrbc_value members = instance_data->members;
  int size = members.hash->n_stored / 2;
  mrbc_value keys = mrbc_array_new(vm, size);
  for (int i = 0; i < size; i++) {
    mrbc_value key = members.hash->data[i * 2];
    mrbc_array_set(&keys, i, &key);
  }
  SET_RETURN(keys);
}

static void
c_instance_is_a_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value arg = GET_ARG(1);
  int result;
  if (arg.tt == MRBC_TT_CLASS) {
    result = mrbc_obj_is_kind_of(v, arg.cls);
  } else if (arg.tt == MRBC_TT_OBJECT) {
    /* A definition is not a class: compare it with the one of the instance. */
    data_instance_t *instance_data = (data_instance_t *)v->instance->data;
    result = (arg.instance == instance_data->definition.instance);
  } else {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "not a class");
    return;
  }
  if (result) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_instance_inspect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value str = mrbc_string_new_cstr(vm, "#<data>");
  SET_RETURN(str);
}

/*
 * Subclass Class methods
 */

static void
c_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!is_definition(v)) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method 'new'");
    return;
  }
  data_subclass_t *subclass_data = (data_subclass_t *)v->instance->data;
  if (subclass_data->member_keys.array->data_size != argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value self = mrbc_instance_new(vm, class_Data_Instance, sizeof(data_instance_t));
  data_instance_t *instance_data = (data_instance_t *)self.instance->data;
  memset(instance_data, 0, sizeof(data_instance_t));

  mrbc_value members = mrbc_hash_new(vm, argc);
  for (int i = 0; i < argc; i++) {
    mrbc_value key = mrbc_array_get(&subclass_data->member_keys, i);
    mrbc_value value = GET_ARG(i + 1);
    mrbc_hash_set(&members, &key, &value);
    mrbc_incref(&value);
  }
  instance_data->members = members;
  mrbc_incref(&v[0]);
  instance_data->definition = v[0];
  SET_RETURN(self);
}

static void
c_members(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!is_definition(v)) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method 'members'");
    return;
  }
  data_subclass_t *subclass_data = (data_subclass_t *)v->instance->data;
  /* Return a copy so that the caller cannot change the definition. */
  mrbc_value member_keys = mrbc_array_dup(vm, &subclass_data->member_keys);
  SET_RETURN(member_keys);
}

/*
 * Data class Class methods
 */

static void
c_define(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v->tt != MRBC_TT_CLASS || v->cls != class_Data) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method 'define'");
    return;
  }
  /* Check the arguments before any allocation: nothing to release on error. */
  for (int i = 0; i < argc; i++) {
    mrbc_vtype tt = GET_ARG(i + 1).tt;
    if (tt != MRBC_TT_STRING && tt != MRBC_TT_SYMBOL) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "not a symbol nor a string");
      return;
    }
  }

  mrbc_value subclass = mrbc_instance_new(vm, class_Data, sizeof(data_subclass_t));
  data_subclass_t *data = (data_subclass_t *)subclass.instance->data;
  memset(data, 0, sizeof(data_subclass_t));

  mrbc_value member_keys = mrbc_array_new(vm, argc);
  for (int i = 0; i < argc; i++) {
    mrbc_value arg = GET_ARG(i + 1);
    mrbc_value key = arg;
    if (arg.tt == MRBC_TT_STRING) {
      /* mrbc_symbol_new copies the name. mrbc_str_to_symid keeps the pointer,
         which dangles when the String is released. */
      key = mrbc_symbol_new(vm, (const char *)arg.string->data);
    }
    mrbc_array_set(&member_keys, i, &key);
  }
  data->member_keys = member_keys;
  SET_RETURN(subclass);
}

void
mrbc_data_init(mrbc_vm *vm)
{
  class_Data = mrbc_define_class(vm, "Data", mrbc_class_object);
  mrbc_define_destructor(class_Data, data_subclass_destructor);

  mrbc_define_method(vm, class_Data, "define", c_define);
  mrbc_define_method(vm, class_Data, "new", c_new);
  mrbc_define_method(vm, class_Data, "members", c_members);

  class_Data_Instance = mrbc_define_class_under(vm, class_Data, "Instance", class_Data);
  mrbc_define_destructor(class_Data_Instance, data_instance_destructor);

  mrbc_define_method(vm, class_Data_Instance, "method_missing", c_method_missing);
  mrbc_define_method(vm, class_Data_Instance, "members", c_instance_members);
  mrbc_define_method(vm, class_Data_Instance, "to_h", c_instance_to_h);
  mrbc_define_method(vm, class_Data_Instance, "is_a?", c_instance_is_a_q);
  mrbc_define_method(vm, class_Data_Instance, "inspect", c_instance_inspect);
}
