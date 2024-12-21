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

EM_ASYNC_JS(int, await_promise, (int promise_id), {
  try {
    const promise = window.picorubyRefs[promise_id];
    const result = await promise;
    const resultId = window.picorubyRefs.length;
    window.picorubyRefs.push(result);
    return resultId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

//EM_ASYNC_JS(void, js_add_event_listener, (int ref_id, const char* event_type), {
EM_JS(void, js_add_event_listener, (int ref_id, uintptr_t callback_id, const char* event_type), {
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
      ['number', 'number', 'string'],
      [ref_id, callback_id, eventStr]
    );
  });
});

EM_ASYNC_JS(char*, array_buffer_to_binary, (int ref_id), {
  try {
    const response = window.picorubyRefs[ref_id];
    const arrayBuffer = await response.arrayBuffer();
    const uint8Array = new Uint8Array(arrayBuffer);
    const ptr = _malloc(uint8Array.length);
    const heapBytes = new Uint8Array(HEAPU8.buffer, ptr, uint8Array.length);
    heapBytes.set(uint8Array);
    return ptr;
  } catch(e) {
    console.error('Error:', e);
    return null;
  }
});

typedef struct {
  enum {
    TYPE_UNDEFINED,
    TYPE_NULL,
    TYPE_BOOLEAN,
    TYPE_NUMBER,
    TYPE_BIGINT,
    TYPE_STRING,
    TYPE_SYMBOL,
    TYPE_ARRAY,
    TYPE_OBJECT,
    TYPE_FUNCTION
  } type;
  union {
    bool boolean;
    double number;
    int array_length;
  } value;
  bool is_integer;
  char* string_value;
} js_type_info;

// Functions calcluate the offsets of the fields in js_type_info struct
static const size_t TYPE_OFFSET = offsetof(js_type_info, type);
static const size_t VALUE_OFFSET = offsetof(js_type_info, value);
static const size_t IS_INTEGER_OFFSET = offsetof(js_type_info, is_integer);
static const size_t STRING_VALUE_OFFSET = offsetof(js_type_info, string_value);

EM_JS(void, init_js_type_offsets, (size_t type_offset, size_t value_offset,
                                  size_t is_integer_offset, size_t string_value_offset), {
  globalThis.JS_TYPE_INFO_OFFSETS = {
    type: type_offset,
    value: value_offset,
    is_integer: is_integer_offset,
    string_value: string_value_offset
  };
});

EM_JS(void, js_get_type_info, (int ref_id, js_type_info* info), {
  const value = globalThis.picorubyRefs[ref_id];
  const offsets = globalThis.JS_TYPE_INFO_OFFSETS;
  let type = typeof value;
  
  if (value === null) {
    HEAP32[info + offsets.type >> 2] = 1;  // TYPE_NULL
    HEAP32[info + offsets.string_value >> 2] = 0;
  } else if (Array.isArray(value)) {
    HEAP32[info + offsets.type >> 2] = 7;  // TYPE_ARRAY
    HEAP32[info + offsets.value >> 2] = value.length;
    HEAP32[info + offsets.string_value >> 2] = 0;
  } else {
    switch(type) {
      case "undefined":
        HEAP32[info + offsets.type >> 2] = 0;
        HEAP32[info + offsets.string_value >> 2] = 0;
        break;
      case "boolean":
        HEAP32[info + offsets.type >> 2] = 2;
        HEAPU8[info + offsets.value] = value ? 1 : 0;
        HEAP32[info + offsets.string_value >> 2] = 0;
        break;
      case "number":
        HEAP32[info + offsets.type >> 2] = 3;
        HEAPF64[info + offsets.value >> 3] = value;
        HEAPU8[info + offsets.is_integer] = Number.isInteger(value) ? 1 : 0;
        HEAP32[info + offsets.string_value >> 2] = 0;
        break;
      case "bigint":
      case "string":
      case "symbol": {
        HEAP32[info + offsets.type >> 2] = type === "bigint" ? 4 :
                                          type === "string" ? 5 : 6;
        const str = type === "symbol" ? (value.description || "") : value.toString();
        const length = lengthBytesUTF8(str) + 1;
        const ptr = _malloc(length);
        stringToUTF8(str, ptr, length);
        HEAP32[info + offsets.string_value >> 2] = ptr;
        break;
      }
      case "object":
        HEAP32[info + offsets.type >> 2] = 8;
        HEAP32[info + offsets.string_value >> 2] = 0;
        break;
      case "function":
        HEAP32[info + offsets.type >> 2] = 9;
        HEAP32[info + offsets.string_value >> 2] = 0;
        break;
    }
  }
});


/*****************************************************
 * callback functions
 *****************************************************/

static char*
callback_script(uintptr_t callback_id, const char* event)
{
  static char script[255];
  snprintf(script, sizeof(script), "JS::Object::CALLBACKS[%lu].call('%s');", callback_id, event);
  return script;
}

EMSCRIPTEN_KEEPALIVE
void
call_ruby_callback(int ref_id, uintptr_t callback_id, const char* event)
{
  char *utf8 = callback_script(callback_id, event);
  mrc_ccontext *c = mrc_ccontext_new(NULL);
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


/*****************************************************
 * methods for JS::Object
 *****************************************************/

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
 * JS::Object#to_binary
 */
static void
c_object_to_binary(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_js_obj *obj = (picorb_js_obj *)v[0].instance->data;
  char *binary = array_buffer_to_binary(obj->ref_id);
  if (!binary) {
    SET_NIL_RETURN();
    return;
  }
  mrbc_value str = mrbc_string_new(vm, binary, strlen(binary));
  SET_RETURN(str);
}

/*
 * JS::Object#to_poro
 */
static void
c_object_to_poro(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_js_obj *obj = (picorb_js_obj *)v[0].instance->data;
  js_type_info info = { 0 };
  js_get_type_info(obj->ref_id, &info);

  switch (info.type) {
    case TYPE_UNDEFINED:
    case TYPE_NULL:
      SET_NIL_RETURN();
      break;
    case TYPE_BOOLEAN:
      if (info.value.boolean) {
        SET_TRUE_RETURN();
      } else {
        SET_FALSE_RETURN();
      }
      break;
    case TYPE_NUMBER:
      if (info.is_integer) {
        SET_INT_RETURN((int32_t)info.value.number);
      } else {
        SET_FLOAT_RETURN(info.value.number);
      }
      break;
    case TYPE_BIGINT:
      if (info.string_value) {
        SET_INT_RETURN(atoi(info.string_value));
        free(info.string_value);
      } else {
        SET_INT_RETURN(0);
      }
      break;
    case TYPE_STRING:
      if (info.string_value) {
        SET_RETURN(mrbc_string_new_cstr(vm, info.string_value));
        free(info.string_value);
      } else {
        SET_RETURN(mrbc_string_new_cstr(vm, ""));
      }
      break;
    case TYPE_SYMBOL:
      if (info.string_value) {
        SET_RETURN(mrbc_symbol_value(mrbc_str_to_symid(info.string_value)));
        free(info.string_value);
      } else {
        SET_RETURN(mrbc_symbol_value(mrbc_str_to_symid("")));
      }
      break;
    case TYPE_ARRAY:
      {
        mrbc_value array = mrbc_array_new(vm, info.value.array_length);
        // No recursive conversion for now
        for (int i = 0; i < info.value.array_length; i++) {
          mrbc_value element = mrbc_instance_new(vm, class_JS_Object, sizeof(picorb_js_obj));
          picorb_js_obj *data = (picorb_js_obj *)element.instance->data;
          char key[10];
          snprintf(key, sizeof(key), "%d", i);
          data->ref_id = get_property(obj->ref_id, key);
          mrbc_array_set(&array, i, &element);
        }
        SET_RETURN(array);
      }
      break;
    case TYPE_OBJECT:
    case TYPE_FUNCTION:
      SET_RETURN(v[0]);
      break;
  }
}

/*
 * JS::Object#awit
 */
static void
c_object_await(mrbc_vm *vm, mrbc_value v[], int argc)
{
  picorb_js_obj *obj = (picorb_js_obj *)v[0].instance->data;
  int new_ref_id = await_promise(obj->ref_id);
  if (new_ref_id < 0) {
    SET_NIL_RETURN();
    return;
  }
  mrbc_value new_obj = mrbc_instance_new(vm, class_JS_Object, sizeof(picorb_js_obj));
  picorb_js_obj *data = (picorb_js_obj *)new_obj.instance->data;
  data->ref_id = new_ref_id;
  SET_RETURN(new_obj);
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
  uintptr_t callback_id = (uintptr_t)GET_INT_ARG(1);
  const char* event_type = (const char*)GET_STRING_ARG(2);
  js_add_event_listener(obj->ref_id, callback_id, event_type);
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
  init_js_type_offsets(TYPE_OFFSET, VALUE_OFFSET, IS_INTEGER_OFFSET, STRING_VALUE_OFFSET);

  mrbc_class *module_JS = mrbc_define_module(vm, "JS");
  mrbc_define_method(vm, module_JS, "global", c_global);

  class_JS_Object = mrbc_define_class_under(vm, module_JS, "Object", mrbc_class_object);
  mrbc_define_method(vm, class_JS_Object, "[]", c_object_get_property);
  mrbc_define_method(vm, class_JS_Object, "method_missing", c_object_method_missing);
  mrbc_define_method(vm, class_JS_Object, "_add_event_listener", c_object__add_event_listener);
  mrbc_define_method(vm, class_JS_Object, "await", c_object_await);
  mrbc_define_method(vm, class_JS_Object, "to_binary", c_object_to_binary);
  mrbc_define_method(vm, class_JS_Object, "to_poro", c_object_to_poro);
}

