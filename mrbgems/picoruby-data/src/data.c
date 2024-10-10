#include <mrubyc.h>
#include <stdio.h>

typedef struct DataSubclass {
  mrbc_class *cls;
  mrbc_value member_keys;
} data_subclass_t;

typedef struct DataInstance {
  mrbc_value members;
} data_instance_t;

static mrbc_class *class_Data;

/*
 * Instance methods
 */

static void
c_member_reader(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_value key = mrbc_symbol_value(vm->exception.i);
  data_instance_t *instance_data = (data_instance_t *)v->instance->data;
  mrbc_value members = instance_data->members;
  mrbc_value value = mrbc_hash_get(&members, &key);
  SET_RETURN(value);
}

static void
c_instance_to_h(mrbc_vm *vm, mrbc_value *v, int argc)
{
  data_instance_t *instance_data = (data_instance_t *)v->instance->data;
  mrbc_value members = instance_data->members;
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

/*
 * Subclass Class methods
 */

static void
c_new(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v->cls == class_Data) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method 'new' for class Data");
    return;
  }
  data_subclass_t *subclass_data = (data_subclass_t *)v->instance->data;
  if (subclass_data->member_keys.array->data_size != argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  mrbc_value self = mrbc_instance_new(vm, subclass_data->cls, sizeof(data_subclass_t));
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
  SET_RETURN(self);
}

static void
c_members(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v->cls == class_Data) {
    mrbc_raise(vm, MRBC_CLASS(NoMethodError), "undefined method 'members' for class Data");
    return;
  }
  data_subclass_t *subclass_data = (data_subclass_t *)v->instance->data;
  mrbc_value member_keys = subclass_data->member_keys;
  SET_RETURN(member_keys);
}

/*
 * Data class Class methods
 */

static void
c_define(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (vm->exception.tt != MRBC_TT_SYMBOL) {
    mrbc_raise(vm, MRBC_CLASS(NotImplementedError), "Data.define is not implemented");
    return;
  }

  mrbc_value subclass = mrbc_instance_new(vm, v->cls, sizeof(data_subclass_t));
  data_subclass_t *data = (data_subclass_t *)subclass.instance->data;
  memset(data, 0, sizeof(data_subclass_t));

  char buf[24] = {0};
  memcpy(buf, "Data_", 5);
  sprintf(buf + 5, "0x%p", &subclass.instance);
  mrbc_class *cls = mrbc_define_class(vm, buf, mrbc_class_object);

  mrbc_value member_keys = mrbc_array_new(vm, argc);
  for (int i = 0; i < argc; i++) {
    mrbc_value arg = GET_ARG(i + 1);
    mrbc_value name;
    mrbc_value key;
    switch (arg.tt) {
      case MRBC_TT_STRING: {
        name = arg;
        key = mrbc_symbol_value(mrbc_str_to_symid((const char *)arg.string->data));
        break;
        }
      case MRBC_TT_SYMBOL: {
        name = mrbc_string_new_cstr(vm, mrbc_symid_to_str(arg.i));
        key = arg;
        break;
        }
      default: {
        mrbc_raise(vm, MRBC_CLASS(TypeError), "not a symbol nor a string");
        return;
      }
    }
    mrbc_array_set(&member_keys, i, &key);
    mrbc_define_method(vm, cls, (const char *)name.string->data, c_member_reader);
  }
  mrbc_define_method(vm, cls, "members", c_instance_members);
  mrbc_define_method(vm, cls, "to_h", c_instance_to_h);

  data->cls = cls;
  data->member_keys = member_keys;
  SET_RETURN(subclass);
}

void
mrbc_data_init(mrbc_vm *vm)
{
  class_Data = mrbc_define_class(vm, "Data", mrbc_class_object);

  mrbc_define_method(vm, class_Data, "define", c_define);
  mrbc_define_method(vm, class_Data, "new", c_new);
  mrbc_define_method(vm, class_Data, "members", c_members);
}
