#include <emscripten.h>

#include <mrc_common.h>
#include <mrc_ccontext.h>
#include <mrc_compile.h>
#include <mrc_dump.h>
#include <mrubyc.h>

#include <stdbool.h>
#include <string.h>

typedef struct picorb_js_obj {
  int ref_id;
} picorb_js_obj;

static mrbc_class *class_JS_Object;


EM_JS(void, init_js_refs, (), {
  if (typeof globalThis.picorubyRefs === 'undefined') {
    globalThis.picorubyRefs = [];
    globalThis.picorubyRefs.push(window);
  }
});

EM_JS(bool, has_property, (int ref_id, const char* name), {
  const obj = window.picorubyRefs[ref_id];
  return name in obj;
});

EM_JS(bool, set_property, (int ref_id, const char* key, const char* value), {
  try {
    if (!globalThis.picorubyRefs || ref_id >= globalThis.picorubyRefs.length) {
      console.error('Invalid reference ID:', ref_id);
      return false;
    }

    const obj = globalThis.picorubyRefs[ref_id];
    if (!obj) {
      console.error('Object not found for ref_id:', ref_id);
      return false;
    }

    const property = UTF8ToString(key);
    const val = UTF8ToString(value);
    obj[property] = val;
    return true;
  } catch(e) {
    console.error('Error in set_property:', e);
    return false;
  }
});

EM_JS(int, get_property, (int ref_id, const char* key), {
  try {
    const obj = window.picorubyRefs[ref_id];
    const value = obj[UTF8ToString(key)];
    const newRefId = window.picorubyRefs.length;
    window.picorubyRefs.push(value);
    return newRefId;
  } catch(e) {
    return -1;
  }
});

EM_JS(int, call_method, (int ref_id, const char* method, const char* arg), {
  try {
    const obj = window.picorubyRefs[ref_id];
    const func = obj[UTF8ToString(method)];
    const result = func.call(obj, UTF8ToString(arg));
    const newRefId = window.picorubyRefs.length;
    window.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

//EM_ASYNC_JS(void, js_add_event_listener, (int ref_id, const char* event_type), {
EM_JS(void, js_add_event_listener, (int ref_id, const char* event_type), {
  const target = globalThis.picorubyRefs[ref_id];
  const type = UTF8ToString(event_type);
  target.addEventListener(type, (event) => {
    const eventStr = JSON.stringify({
      type: event.type,
     // timeStamp: event.timeStamp
    });
    ccall(
      'call_ruby_callback',
      'void',
      ['number', 'string', 'string'],
      [ref_id, eventStr]
    );
  });
});

EMSCRIPTEN_KEEPALIVE
void
call_ruby_callback(int ref_id, const char* event)
{
  mrbc_value event_json = mrbc_string_new_cstr(NULL, event);
  mrbc_set_global(mrbc_str_to_symid("$event_json"), &event_json);
  mrc_ccontext *c = mrc_ccontext_new(NULL);
  const char *utf8 = "JS::Object::CALLBACKS[0].call($event_json)";
  mrc_irep *irep = mrc_load_string_cxt(c, (const uint8_t **)&utf8, strlen(utf8));
  int result;
  uint8_t *mrb = NULL;
  size_t mrb_size = 0;
  result = mrc_dump_irep(c, irep, 0, &mrb, &mrb_size);
  if (result != MRC_DUMP_OK) {
    console_printf("Failed to dump irep\n");
    return;
  }
  mrc_irep_free(c, irep);
  mrc_ccontext_free(c);
  mrbc_tcb *tcb = mrbc_create_task(mrb, NULL);
  (void)tcb;
}

/*
 * JS::Object#[]
 */
static void
c_object_get_property(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_js_obj *parent = (picorb_js_obj *)v[0].instance->data;
  const mrbc_sym key = v[1].i;
  const char* key_str = mrbc_symid_to_str(key);
  int ref_id = get_property(parent->ref_id, key_str);

  if (ref_id < 0) {
    SET_NIL_RETURN();
    return;
  }

  mrbc_value obj = mrbc_instance_new(vm, class_JS_Object, sizeof(picorb_js_obj));
  picorb_js_obj *data = (picorb_js_obj *)obj.instance->data;
  data->ref_id = ref_id;
  SET_RETURN(obj);
}

/*
 * JS::Object#method_missing
 */
static void
c_object_method_missing(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value self = v[0];
  mrbc_sym method_sym = v[1].i;
  const char *method_name = mrbc_symid_to_str(method_sym);
  //console_printf("method_missing: %s\n", method_name);
  picorb_js_obj *js_obj = (picorb_js_obj *)self.instance->data;

  //if (!has_property(js_obj->ref_id, method_name)) {
  //  mrbc_raisef(vm, MRBC_CLASS(NoMethodError), "undefined method '%s'", method_name);
  //  return;
  //}

  if (method_name[strlen(method_name) - 1] == '=') {
    if (argc != 2) {
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
      return;
    }
    char property_name[100];
    strncpy(property_name, method_name, strlen(method_name) - 1);
    property_name[strlen(method_name) - 1] = '\0';
    bool success = set_property(js_obj->ref_id, property_name, (const char *)GET_STRING_ARG(2));
    if (!success) {
      SET_NIL_RETURN();
      return;
    }
    mrbc_incref(&v[2]);
    SET_RETURN(v[2]);
    return;
  }

  if (argc == 1) { // No argument
    int new_ref_id = call_method(js_obj->ref_id, method_name, "");
    if (new_ref_id < 0) {
      SET_NIL_RETURN();
      return;
    }
  } else if (argc == 2) { // One argument
    if (v[2].tt == MRBC_TT_STRING) {
      int new_ref_id = call_method(js_obj->ref_id, method_name, (const char *)GET_STRING_ARG(2));
      if (new_ref_id < 0) {
        SET_NIL_RETURN();
        return;
      }
      mrbc_value obj = mrbc_instance_new(vm, class_JS_Object, sizeof(picorb_js_obj));
      picorb_js_obj *data = (picorb_js_obj *)obj.instance->data;
      data->ref_id = new_ref_id;
      SET_RETURN(obj);
    } else {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "argument must be a string");
    }
  } else {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
  }
}


/*
 * JS::Object#_add_event_listener
 */
static void
c_object__add_event_listener(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_js_obj *obj = (picorb_js_obj *)v[0].instance->data;
  const char* event_type = (const char*)GET_STRING_ARG(1);
  js_add_event_listener(obj->ref_id, event_type);
  SET_NIL_RETURN();
}


/*
 * JS.global
 */
static void
c_global(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value global = mrbc_instance_new(vm, class_JS_Object, sizeof(picorb_js_obj));
  picorb_js_obj *data = (picorb_js_obj *)global.instance->data;
  data->ref_id = 0;
  SET_RETURN(global);
}


void
mrbc_js_init(mrbc_vm *vm)
{
  init_js_refs();

  mrbc_class *module_JS = mrbc_define_module(vm, "JS");
  mrbc_define_method(vm, module_JS, "global", c_global);

  class_JS_Object = mrbc_define_class_under(vm, module_JS, "Object", mrbc_class_object);
  mrbc_define_method(vm, class_JS_Object, "[]", c_object_get_property);
  mrbc_define_method(vm, class_JS_Object, "method_missing", c_object_method_missing);
  mrbc_define_method(vm, class_JS_Object, "_add_event_listener", c_object__add_event_listener);
}

