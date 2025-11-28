#include <emscripten.h>

#include "mruby.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/compile.h"
#include "mruby/data.h"
#include "mruby/hash.h"
#include "mruby/numeric.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#include "mruby_compiler.h"
#include "task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct picorb_js_obj {
  int ref_id;
} picorb_js_obj;

static struct RClass *class_JS_Object;

static void
picorb_js_obj_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

static const struct mrb_data_type picorb_js_obj_type = {
  "picorb_js_obj", picorb_js_obj_free
};

EM_JS(void, init_js_refs, (), {
  if (typeof globalThis.picorubyRefs === 'undefined') {
    globalThis.picorubyRefs = [];
    globalThis.picorubyRefs.push(window);
  }
});

EM_JS(bool, is_array_like, (int ref_id), {
  const obj = window.picorubyRefs[ref_id];
  // NodeList or HTMLCollection
  return obj instanceof NodeList ||
         obj instanceof HTMLCollection ||
         // Just in case, check the length property and numeric index access
         (typeof obj === 'object' &&
          obj !== null &&
          'length' in obj &&
          typeof obj.length === 'number');
});

EM_JS(int, get_element, (int ref_id, int index), {
  try {
    const nodeList = window.picorubyRefs[ref_id];
    const element = nodeList[index];
    if (element === undefined) {
      return -1;
    }
    const newRefId = window.picorubyRefs.push(element) - 1;
    return newRefId;
  } catch(e) {
    return -1;
  }
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

EM_JS(int, get_length, (int ref_id), {
  try {
    const obj = window.picorubyRefs[ref_id];
    return obj.length || 0;
  } catch(e) {
    return -1;
  }
})

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

EM_JS(int, call_method_str, (int ref_id, const char* method, const char* arg1, const char *arg2), {
  try {
    const obj = window.picorubyRefs[ref_id];
    const func = obj[UTF8ToString(method)];
    const result = func.call(obj, UTF8ToString(arg1), UTF8ToString(arg2));
    const newRefId = window.picorubyRefs.length;
    window.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

EM_JS(int, call_method_with_ref, (int ref_id, const char* method, int arg_ref_id), {
  try {
    const obj = window.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];

    const argObj = window.picorubyRefs[arg_ref_id];
    const result = func.call(obj, argObj);

    const newRefId = window.picorubyRefs.length;
    window.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

EM_JS(int, call_method_with_ref_ref, (int ref_id, const char* method, int arg_ref_1_id, int arg_ref_2_id), {
  try {
    const obj = window.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];

    const argObj1 = window.picorubyRefs[arg_ref_1_id];
    const argObj2 = window.picorubyRefs[arg_ref_2_id];
    const result = func.call(obj, argObj1, argObj2);

    const newRefId = window.picorubyRefs.length;
    window.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

EM_JS(void, setup_promise_handler, (int promise_id, uintptr_t callback_id, uintptr_t mrb_ptr, uintptr_t task_ptr), {
  const promise = globalThis.picorubyRefs[promise_id];
  promise.then(
    (result) => {
      const resultId = globalThis.picorubyRefs.length;
      globalThis.picorubyRefs.push(result);
      ccall(
        'resume_promise_task',
        'void',
        ['number', 'number', 'number', 'number'],
        [mrb_ptr, task_ptr, callback_id, resultId]
      );
    }
//  ).catch(
//    (error) => {
//      console.error('Promise rejected:', error);
//      const errorString = (error && error.toString) ? error.toString() : 'Unknown error';
//      const errPtr = stringToUTF8(errorString);
//      ccall(
//        'resume_promise_error_task',
//        'void',
//        ['number', 'number', 'number', 'number'],
//        [mrb_ptr, task_ptr, callback_id, errPtr]
//      );
//    }
  );
});

EM_JS(void, js_add_event_listener, (int ref_id, uintptr_t callback_id, const char* event_type), {
  const target = globalThis.picorubyRefs[ref_id];
  const type = UTF8ToString(event_type);
  target.addEventListener(type, (event) => {
    const eventRefId = globalThis.picorubyRefs.push(event) - 1;
    ccall(
      'call_ruby_callback',
      'void',
      ['number', 'number'],
      [callback_id, eventRefId]
    );
  });
});

EM_JS(int, setup_binary_handler, (int ref_id, uintptr_t mrb_ptr, uintptr_t task_ptr, uintptr_t callback_id), {
  const response = window.picorubyRefs[ref_id];
  if (!response || typeof response.arrayBuffer !== 'function') {
    console.error('Invalid response object:', response);
    return 0;
  }
  response.arrayBuffer().then(arrayBuffer => {
    const uint8Array = new Uint8Array(arrayBuffer);
    const ptr = _malloc(uint8Array.length);
    const heapBytes = new Uint8Array(HEAPU8.buffer, ptr, uint8Array.length);
    heapBytes.set(uint8Array);
    ccall(
      'resume_binary_task',
      'void',
      ['number', 'number', 'number', 'number', 'number'],
      [mrb_ptr, task_ptr, callback_id, ptr, uint8Array.length]
    );
  }).catch(error => {
    console.error('Error in arrayBuffer processing:', error);
  });
  return 0;
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

#ifdef __cplusplus
enum { TYPE_UNDEFINED, TYPE_NULL, TYPE_BOOLEAN, TYPE_NUMBER, TYPE_BIGINT,
       TYPE_STRING, TYPE_SYMBOL, TYPE_ARRAY, TYPE_OBJECT, TYPE_FUNCTION };
#endif

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

inline static char*
callback_script(uintptr_t callback_id, int event_ref_id)
{
  static char script[255];
  snprintf(script, sizeof(script), "JS::Object::CALLBACKS[%lu].call($js_events[%d]);", callback_id, event_ref_id);
  return script;
}

extern mrb_state *global_mrb;
extern mrb_value main_task;

EMSCRIPTEN_KEEPALIVE
void
call_ruby_callback(uintptr_t callback_id, int event_ref_id)
{
  if (!global_mrb) {
    return;
  }

  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(global_mrb, sizeof(picorb_js_obj));
  data->ref_id = event_ref_id;
  mrb_value event = mrb_obj_value(Data_Wrap_Struct(global_mrb, class_JS_Object, &picorb_js_obj_type, data));

  mrb_value events = mrb_gv_get(global_mrb, MRB_GVSYM(js_events));
  if (mrb_nil_p(events)) {
    events = mrb_hash_new(global_mrb);
    mrb_gv_set(global_mrb, MRB_GVSYM(js_events), events);
  }
  mrb_hash_set(global_mrb, events, mrb_fixnum_value(event_ref_id), event);

  char *script = callback_script(callback_id, event_ref_id);

  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  const uint8_t *script_ptr = (const uint8_t *)script;
  size_t size = strlen(script);
  mrc_irep *irep = mrc_load_string_cxt(cc, &script_ptr, size);

  if (!irep) {
    mrc_ccontext_free(cc);
    return;
  }

  mrb_value task = mrc_create_task(cc, irep, mrb_nil_value(), mrb_nil_value(), mrb_obj_value(global_mrb->object_class));

  if (!mrb_nil_p(task)) {
    mrb_tcb *tcb = (mrb_tcb *)mrb_data_get_ptr(global_mrb, task, &mrb_task_tcb_type);
    tcb->irep = irep;
    tcb->cc = cc;
  } else {
    mrc_irep_free(cc, irep);
    mrc_ccontext_free(cc);
    return;
  }

  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    fprintf(stderr, "Callback exception: %s\n", RSTRING_PTR(exc_str));
    global_mrb->exc = NULL;
  }
}

EMSCRIPTEN_KEEPALIVE
void
resume_promise_task(uintptr_t mrb_ptr, uintptr_t task_ptr, uintptr_t callback_id, int result_id)
{
  mrb_state *mrb = (mrb_state *)mrb_ptr;
  if (!mrb) {
    fprintf(stderr, "DEBUG: mrb is NULL\n");
    return;
  }

  mrb_value responses = mrb_gv_get(mrb, MRB_GVSYM(promise_responses));
  if (mrb_nil_p(responses)) {
    responses = mrb_hash_new(mrb);
    mrb_gv_set(mrb, MRB_GVSYM(promise_responses), responses);
  }

  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
  data->ref_id = result_id;
  mrb_value response = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));

  mrb_hash_set(mrb, responses, mrb_fixnum_value(callback_id), response);

  mrb_value task = mrb_obj_value((struct RBasic *)task_ptr);
  mrb_resume_task(mrb, task);
}

//EMSCRIPTEN_KEEPALIVE
//void
//resume_promise_error_task(uintptr_t mrb_ptr, uintptr_t task_ptr, uintptr_t callback_id, char *errmsg)
//{
//  fprintf(stderr, "Promise rejected: %s\n", errmsg);
//  mrb_state *mrb = (mrb_state *)mrb_ptr;
//  if (!mrb) return;
//  mrb_value exc = mrb_exc_new_str(mrb, E_RUNTIME_ERROR, mrb_str_new_cstr(mrb, errmsg));
//  mrb_value task = mrb_obj_value((struct RBasic *)task_ptr);
//  mrb_resume_task_with_raise(mrb, task, exc);
//  free(errmsg);
//}

EMSCRIPTEN_KEEPALIVE
void
resume_binary_task(uintptr_t mrb_ptr, uintptr_t task_ptr, uintptr_t callback_id, void *binary, int length)
{
  mrb_state *mrb = (mrb_state *)mrb_ptr;
  if (!mrb) {
    free(binary);
    return;
  }

  mrb_value responses = mrb_gv_get(mrb, MRB_GVSYM(promise_responses));
  if (mrb_nil_p(responses)) {
    responses = mrb_hash_new(mrb);
    mrb_gv_set(mrb, MRB_GVSYM(promise_responses), responses);
  }

  mrb_value str = mrb_str_new(mrb, (const char *)binary, length);
  mrb_hash_set(mrb, responses, mrb_fixnum_value(callback_id), str);
  free(binary);

  mrb_value task = mrb_obj_value((struct RBasic *)task_ptr);
  mrb_resume_task(mrb, task);
}


/*****************************************************
 * methods for JS::Object
 *****************************************************/

/*
 * JS::Object#[]
 */
static mrb_value
mrb_object_get_property(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *parent = (picorb_js_obj *)DATA_PTR(self);
  mrb_sym key;
  mrb_get_args(mrb, "n", &key);
  const char* key_str = mrb_sym_name(mrb, key);
  int ref_id = get_property(parent->ref_id, key_str);

  if (ref_id < 0) {
    return mrb_nil_value();
  }

  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
  data->ref_id = ref_id;
  mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
  return obj;
}

/*
 * JS::Object#to_binary
 */
static mrb_value
mrb_object__to_binary_and_suspend(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(self);
  mrb_int callback_id;
  mrb_get_args(mrb, "i", &callback_id);

  mrb_value current_task = mrb_funcall_id(mrb, mrb_obj_value(mrb_class_get_id(mrb, MRB_SYM(Task))), MRB_SYM(current), 0);

  uintptr_t task_ptr = (uintptr_t)mrb_val_union(current_task).vp;

  mrb_suspend_task(mrb, current_task);
  setup_binary_handler(js_obj->ref_id, (uintptr_t)mrb, task_ptr, (uintptr_t)callback_id);

  return mrb_nil_value();
}

/*
 * JS::Object#to_poro
 */
static mrb_value
mrb_object_to_poro(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  js_type_info info = {};
  js_get_type_info(obj->ref_id, &info);

  switch (info.type) {
    case TYPE_UNDEFINED:
    case TYPE_NULL:
      return mrb_nil_value();
    case TYPE_BOOLEAN:
      if (info.value.boolean) {
        return mrb_true_value();
      } else {
        return mrb_false_value();
      }
    case TYPE_NUMBER:
      if (info.is_integer) {
        return mrb_fixnum_value((mrb_int)info.value.number);
      } else {
        return mrb_float_value(mrb, info.value.number);
      }
    case TYPE_BIGINT:
      if (info.string_value) {
        mrb_int val = atoi(info.string_value);
        free(info.string_value);
        return mrb_fixnum_value(val);
      } else {
        return mrb_fixnum_value(0);
      }
    case TYPE_STRING:
      if (info.string_value) {
        mrb_value str = mrb_str_new_cstr(mrb, info.string_value);
        free(info.string_value);
        return str;
      } else {
        return mrb_str_new_cstr(mrb, "");
      }
    case TYPE_SYMBOL:
      if (info.string_value) {
        mrb_sym sym = mrb_intern_cstr(mrb, info.string_value);
        free(info.string_value);
        return mrb_symbol_value(sym);
      } else {
        return mrb_symbol_value(mrb_intern_cstr(mrb, ""));
      }
    case TYPE_ARRAY:
      {
        mrb_value array = mrb_ary_new_capa(mrb, info.value.array_length);
        // No recursive conversion for now
        for (int i = 0; i < info.value.array_length; i++) {
          picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
          char key[10];
          snprintf(key, sizeof(key), "%d", i);
          data->ref_id = get_property(obj->ref_id, key);
          mrb_value element = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
          mrb_ary_push(mrb, array, element);
        }
        return array;
      }
    case TYPE_OBJECT:
      {
        // Create an Array if it is nodeList, otherwise create a Hash
        if (is_array_like(obj->ref_id)) {
          int length = get_length(obj->ref_id);
          mrb_value array = mrb_ary_new_capa(mrb, length);
          for (int i = 0; i < length; i++) {
            int element_ref_id = get_element(obj->ref_id, i);
            if (element_ref_id < 0) {
              mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to get element");
              return mrb_nil_value();
            }
            picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
            data->ref_id = element_ref_id;
            mrb_value element = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
            mrb_ary_push(mrb, array, element);
          }
          return array;
        } else {
          mrb_value hash = mrb_hash_new(mrb);
          return hash;
        }
      }
    case TYPE_FUNCTION:
      return self;
  }

  return mrb_nil_value();
}

/*
 * JS::Object#method_missing
 */
static mrb_value
mrb_object_method_missing(mrb_state *mrb, mrb_value self)
{
  mrb_sym method_sym;
  mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "n*", &method_sym, &argv, &argc);
  const char *method_name = mrb_sym_name(mrb, method_sym);
  picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(self);

  if (method_name[strlen(method_name) - 1] == '=') {
    if (argc != 1) {
      mrb_raise(mrb, E_ARGUMENT_ERROR, "wrong number of arguments");
      return mrb_nil_value();
    }
    char property_name[100];
    strncpy(property_name, method_name, strlen(method_name) - 1);
    property_name[strlen(method_name) - 1] = '\0';
    if (mrb_string_p(argv[0])) {
      bool success = set_property(js_obj->ref_id, property_name, RSTRING_PTR(argv[0]));
      if (!success) {
        return mrb_nil_value();
      }
      return argv[0];
    } else {
      mrb_raise(mrb, E_TYPE_ERROR, "value must be a String");
      return mrb_nil_value();
    }
  }

  int new_ref_id;

  if (argc == 0) { // No argument
    new_ref_id = get_property(js_obj->ref_id, method_name);
    if (new_ref_id >= 0) {
      picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
      data->ref_id = new_ref_id;
      mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
      return obj;
    }
    new_ref_id = call_method(js_obj->ref_id, method_name, "");
    if (new_ref_id < 0) {
      return mrb_nil_value();
    }
  } else if (argc == 1) { // One argument
    if (mrb_string_p(argv[0])) {
      int new_ref_id = call_method(js_obj->ref_id, method_name, RSTRING_PTR(argv[0]));
      if (new_ref_id < 0) {
        return mrb_nil_value();
      }
      picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
      data->ref_id = new_ref_id;
      mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
      return obj;
    } else if (mrb_obj_is_kind_of(mrb, argv[0], class_JS_Object)) {
      picorb_js_obj *arg_obj = (picorb_js_obj *)DATA_PTR(argv[0]);
      int new_ref_id = call_method_with_ref(js_obj->ref_id, method_name, arg_obj->ref_id);
      if (new_ref_id < 0) {
        return mrb_nil_value();
      }
      picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
      data->ref_id = new_ref_id;
      mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
      return obj;
    } else {
      mrb_raise(mrb, E_TYPE_ERROR, "argument must be a String or JS::Object");
      return mrb_nil_value();
    }
  } else if (argc == 2){
    if (mrb_obj_is_kind_of(mrb, argv[0], class_JS_Object) && mrb_obj_is_kind_of(mrb, argv[1], class_JS_Object)) {
      picorb_js_obj *arg_obj_1 = (picorb_js_obj *)DATA_PTR(argv[0]);
      picorb_js_obj *arg_obj_2 = (picorb_js_obj *)DATA_PTR(argv[1]);
      int new_ref_id = call_method_with_ref_ref(js_obj->ref_id, method_name, arg_obj_1->ref_id, arg_obj_2->ref_id);
      if (new_ref_id < 0) {
        return mrb_nil_value();
      }
      picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
      data->ref_id = new_ref_id;
      mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
      return obj;
    } else if (mrb_string_p(argv[0]) && mrb_string_p(argv[1])) {
      int new_ref_id = call_method_str(js_obj->ref_id, method_name, RSTRING_PTR(argv[0]), RSTRING_PTR(argv[1]));
      if (new_ref_id < 0) {
        return mrb_nil_value();
      }
      picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
      data->ref_id = new_ref_id;
      mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
      return obj;
    } else {
      mrb_raisef(mrb, E_TYPE_ERROR, "method: %s, argc: %d", method_name, argc);
      return mrb_nil_value();
    }

  } else {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "method: %s, argc: %d", method_name, argc);
    return mrb_nil_value();
  }

  return mrb_nil_value();
}


/*
 * JS::Object#_add_event_listener
 */
static mrb_value
mrb_object__add_event_listener(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  mrb_int callback_id;
  char* event_type;
  mrb_get_args(mrb, "iz", &callback_id, &event_type);
  js_add_event_listener(obj->ref_id, (uintptr_t)callback_id, event_type);
  return mrb_nil_value();
}


/*
 * JS::Object#_fetch
 */
static mrb_value
mrb_object__fetch_and_suspend(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  char *url;
  mrb_int callback_id;
  mrb_get_args(mrb, "zi", &url, &callback_id);
  int promise_id = call_method(obj->ref_id, "fetch", url);

  mrb_value current_task = mrb_funcall_id(mrb, mrb_obj_value(mrb_class_get_id(mrb, MRB_SYM(Task))), MRB_SYM(current), 0);

  uintptr_t task_ptr = (uintptr_t)mrb_val_union(current_task).vp;

  mrb_suspend_task(mrb, current_task);
  setup_promise_handler(promise_id, (uintptr_t)callback_id, (uintptr_t)mrb, task_ptr);

  return mrb_nil_value();
}

/*
 * JS.global
 */
static mrb_value
mrb_js_global(mrb_state *mrb, mrb_value klass)
{
  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
  data->ref_id = 0;
  mrb_value global = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
  return global;
}

static mrb_value
mrb_js_refcount(mrb_state *mrb, mrb_value self)
{
  // mruby doesn't expose refcount in the same way as mruby/c
  // Return a placeholder value
  return mrb_fixnum_value(0);
}


void
mrb_js_init(mrb_state *mrb)
{
  init_js_refs();
  init_js_type_offsets(TYPE_OFFSET, VALUE_OFFSET, IS_INTEGER_OFFSET, STRING_VALUE_OFFSET);

  struct RClass *module_JS = mrb_define_module(mrb, "JS");
  mrb_define_class_method_id(mrb, module_JS, MRB_SYM(global), mrb_js_global, MRB_ARGS_NONE());

  class_JS_Object = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(Object), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_JS_Object, MRB_TT_DATA);

  mrb_define_method_id(mrb, class_JS_Object, MRB_OPSYM(aref), mrb_object_get_property, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(method_missing), mrb_object_method_missing, MRB_ARGS_ANY());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_add_event_listener), mrb_object__add_event_listener, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_fetch_and_suspend), mrb_object__fetch_and_suspend, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_to_binary_and_suspend), mrb_object__to_binary_and_suspend, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(to_poro), mrb_object_to_poro, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(refcount), mrb_js_refcount, MRB_ARGS_NONE());
}
