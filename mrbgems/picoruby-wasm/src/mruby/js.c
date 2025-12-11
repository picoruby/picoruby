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

  // Helper to remove event listeners from Ruby code
  if (typeof window._js_remove_event_listener_wrapper === 'undefined') {
    window._js_remove_event_listener_wrapper = function(callback_id) {
      if (!globalThis.picorubyEventHandlers) return false;
      const info = globalThis.picorubyEventHandlers[callback_id];
      if (!info) return false;
      info.target.removeEventListener(info.type, info.handler);
      delete globalThis.picorubyEventHandlers[callback_id];
      return true;
    };
  }
});

EM_JS(bool, is_array_like, (int ref_id), {
  const obj = globalThis.picorubyRefs[ref_id];
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
    const nodeList = globalThis.picorubyRefs[ref_id];
    const element = nodeList[index];
    if (element === undefined) {
      return -1;
    }
    const newRefId = globalThis.picorubyRefs.push(element) - 1;
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

EM_JS(bool, set_property_int, (int ref_id, const char* key, int value), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    if (!obj) return false;
    obj[UTF8ToString(key)] = value;
    return true;
  } catch(e) {
    console.error('Error in set_property_int:', e);
    return false;
  }
});

EM_JS(bool, set_property_double, (int ref_id, const char* key, double value), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    if (!obj) return false;
    obj[UTF8ToString(key)] = value;
    return true;
  } catch(e) {
    console.error('Error in set_property_double:', e);
    return false;
  }
});

EM_JS(bool, set_property_bool, (int ref_id, const char* key, bool value), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    if (!obj) return false;
    obj[UTF8ToString(key)] = value ? true : false;
    return true;
  } catch(e) {
    console.error('Error in set_property_bool:', e);
    return false;
  }
});

EM_JS(bool, set_property_null, (int ref_id, const char* key), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    if (!obj) return false;
    obj[UTF8ToString(key)] = null;
    return true;
  } catch(e) {
    console.error('Error in set_property_null:', e);
    return false;
  }
});

EM_JS(bool, set_property_ref, (int ref_id, const char* key, int value_ref_id), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const value = globalThis.picorubyRefs[value_ref_id];
    if (!obj) return false;
    obj[UTF8ToString(key)] = value;
    return true;
  } catch(e) {
    console.error('Error in set_property_ref:', e);
    return false;
  }
});

EM_JS(int, get_property, (int ref_id, const char* key), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const value = obj[UTF8ToString(key)];
    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(value);
    return newRefId;
  } catch(e) {
    return -1;
  }
});

EM_JS(int, get_js_type, (int ref_id), {
  try {
    const value = globalThis.picorubyRefs[ref_id];
    const type = typeof value;
    if (value === null) return 0; // NULL
    if (value === undefined) return 1; // UNDEFINED
    if (type === 'boolean') return 2; // BOOLEAN
    if (type === 'number') return 3; // NUMBER
    if (type === 'string') return 4; // STRING
    if (type === 'function') return 6; // FUNCTION
    return 5; // OBJECT
  } catch(e) {
    return 1; // UNDEFINED
  }
});

EM_JS(int, get_js_property_type, (int ref_id, const char* property_name), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const propName = UTF8ToString(property_name);
    const value = obj[propName];
    const type = typeof value;
    if (value === null) return 0; // NULL
    if (value === undefined) return 1; // UNDEFINED
    if (type === 'boolean') return 2; // BOOLEAN
    if (type === 'number') return 3; // NUMBER
    if (type === 'string') return 4; // STRING
    if (type === 'function') return 6; // FUNCTION
    return 5; // OBJECT
  } catch(e) {
    return 1; // UNDEFINED
  }
});

EM_JS(bool, get_boolean_value, (int ref_id), {
  return globalThis.picorubyRefs[ref_id] ? true : false;
});

EM_JS(double, get_number_value, (int ref_id), {
  return globalThis.picorubyRefs[ref_id];
});

EM_JS(int, get_string_value_length, (int ref_id), {
  const str = globalThis.picorubyRefs[ref_id];
  return lengthBytesUTF8(str);
});

EM_JS(void, copy_string_value, (int ref_id, char* buffer, int buffer_size), {
  const str = globalThis.picorubyRefs[ref_id];
  stringToUTF8(str, buffer, buffer_size);
});

EM_JS(int, get_length, (int ref_id), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    return obj.length || 0;
  } catch(e) {
    return -1;
  }
})

EM_JS(int, call_method, (int ref_id, const char* method, const char* arg), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];
    const argString = UTF8ToString(arg);

    let result;
    if (methodName === 'new') {
      // Call as constructor
      result = new obj(argString);
    } else {
      // Call as method
      result = func.call(obj, argString);
    }

    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

EM_JS(void, call_method_no_return, (int ref_id, const char* method), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];
    if (typeof func === 'function') {
      func.call(obj);
    }
  } catch(e) {
    console.error('call_method_no_return error:', e);
  }
});

EM_JS(int, call_method_int, (int ref_id, const char* method, int arg), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];

    let result;
    if (methodName === 'new') {
      // Call as constructor
      result = new obj(arg);
    } else {
      // Call as method
      result = func.call(obj, arg);
    }

    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

EM_JS(int, call_method_str, (int ref_id, const char* method, const char* arg1, const char *arg2), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];
    const argString1 = UTF8ToString(arg1);
    const argString2 = UTF8ToString(arg2);

    let result;
    if (methodName === 'new') {
      // Call as constructor
      result = new obj(argString1, argString2);
    } else {
      // Call as method
      result = func.call(obj, argString1, argString2);
    }

    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

EM_JS(int, call_method_with_ref, (int ref_id, const char* method, int arg_ref_id), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];

    const argObj = globalThis.picorubyRefs[arg_ref_id];

    let result;
    if (methodName === 'new') {
      // Call as constructor
      result = new obj(argObj);
    } else {
      // Call as method
      result = func.call(obj, argObj);
    }

    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

EM_JS(int, call_method_with_ref_ref, (int ref_id, const char* method, int arg_ref_1_id, int arg_ref_2_id), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];

    const argObj1 = globalThis.picorubyRefs[arg_ref_1_id];
    const argObj2 = globalThis.picorubyRefs[arg_ref_2_id];

    let result;
    if (methodName === 'new') {
      // Call as constructor
      result = new obj(argObj1, argObj2);
    } else {
      // Call as method
      result = func.call(obj, argObj1, argObj2);
    }

    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error(e);
    return -1;
  }
});

EM_JS(int, call_fetch_with_json_options, (int ref_id, const char* url, const char* options_json), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const urlStr = UTF8ToString(url);
    const optionsStr = UTF8ToString(options_json);
    const options = JSON.parse(optionsStr);
    const result = obj.fetch(urlStr, options);
    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
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
  const handler = (event) => {
    // Check if handler still exists before calling
    if (!globalThis.picorubyEventHandlers || !globalThis.picorubyEventHandlers[callback_id]) {
      return;
    }
    // For submit events, prevent default immediately
    if (type === 'submit') {
      event.preventDefault();
    }
    const eventRefId = globalThis.picorubyRefs.push(event) - 1;
    ccall(
      'call_ruby_callback',
      'void',
      ['number', 'number'],
      [callback_id, eventRefId]
    );
  };
  target.addEventListener(type, handler);
  if (!globalThis.picorubyEventHandlers) {
    globalThis.picorubyEventHandlers = {};
  }
  globalThis.picorubyEventHandlers[callback_id] = { target, type, handler };
});

EM_JS(void, js_register_generic_callback, (uintptr_t callback_id, const char* callback_name), {
  const name = UTF8ToString(callback_name);
  if (!globalThis.picorubyGenericCallbacks) {
    globalThis.picorubyGenericCallbacks = {};
  }
  globalThis.picorubyGenericCallbacks[name] = function(...args) {
    // Convert JavaScript arguments to ref_ids
    const argRefIds = args.map(arg => {
      const refId = globalThis.picorubyRefs.push(arg) - 1;
      return refId;
    });

    // Allocate memory for int array
    const argRefIdsPtr = _malloc(argRefIds.length * 4);
    for (let i = 0; i < argRefIds.length; i++) {
      HEAP32[(argRefIdsPtr >> 2) + i] = argRefIds[i];
    }

    // Call Ruby callback synchronously
    const resultRefId = ccall(
      'call_ruby_callback_sync_generic',
      'number',
      ['number', 'number', 'number'],
      [callback_id, argRefIdsPtr, argRefIds.length]
    );

    _free(argRefIdsPtr);

    // Return the result
    if (resultRefId >= 0 && resultRefId < globalThis.picorubyRefs.length) {
      return globalThis.picorubyRefs[resultRefId];
    }
    return undefined;
  };
});


EM_JS(bool, js_remove_event_listener, (uintptr_t callback_id), {
  try {
    if (!globalThis.picorubyEventHandlers) return false;
    const info = globalThis.picorubyEventHandlers[callback_id];
    if (!info) return false;
    info.target.removeEventListener(info.type, info.handler);
    delete globalThis.picorubyEventHandlers[callback_id];
    return true;
  } catch(e) {
    console.error('Error in js_remove_event_listener:', e);
    return false;
  }
});

EM_JS(int, js_create_element, (const char* tag_name), {
  try {
    const element = document.createElement(UTF8ToString(tag_name));
    const refId = globalThis.picorubyRefs.push(element) - 1;
    return refId;
  } catch(e) {
    console.error('Error in js_create_element:', e);
    return -1;
  }
});

EM_JS(int, js_create_text_node, (const char* text), {
  try {
    const node = document.createTextNode(UTF8ToString(text));
    const refId = globalThis.picorubyRefs.push(node) - 1;
    return refId;
  } catch(e) {
    console.error('Error in js_create_text_node:', e);
    return -1;
  }
});

EM_JS(int, js_create_object, (), {
  try {
    const obj = {};
    const refId = globalThis.picorubyRefs.push(obj) - 1;
    return refId;
  } catch(e) {
    console.error('Error in js_create_object:', e);
    return -1;
  }
});

EM_JS(int, js_create_array, (), {
  try {
    const arr = [];
    const refId = globalThis.picorubyRefs.push(arr) - 1;
    return refId;
  } catch(e) {
    console.error('Error in js_create_array:', e);
    return -1;
  }
});

EM_JS(bool, js_append_child, (int parent_ref_id, int child_ref_id), {
  try {
    const parent = globalThis.picorubyRefs[parent_ref_id];
    const child = globalThis.picorubyRefs[child_ref_id];
    if (!parent || !child) return false;
    parent.appendChild(child);
    return true;
  } catch(e) {
    console.error('Error in js_append_child:', e);
    return false;
  }
});

EM_JS(bool, js_remove_child, (int parent_ref_id, int child_ref_id), {
  try {
    const parent = globalThis.picorubyRefs[parent_ref_id];
    const child = globalThis.picorubyRefs[child_ref_id];
    if (!parent || !child) return false;
    parent.removeChild(child);
    return true;
  } catch(e) {
    console.error('Error in js_remove_child:', e);
    return false;
  }
});

EM_JS(bool, js_replace_child, (int parent_ref_id, int new_child_ref_id, int old_child_ref_id), {
  try {
    const parent = globalThis.picorubyRefs[parent_ref_id];
    const newChild = globalThis.picorubyRefs[new_child_ref_id];
    const oldChild = globalThis.picorubyRefs[old_child_ref_id];
    if (!parent || !newChild || !oldChild) return false;
    parent.replaceChild(newChild, oldChild);
    return true;
  } catch(e) {
    console.error('Error in js_replace_child:', e);
    return false;
  }
});

EM_JS(bool, js_insert_before, (int parent_ref_id, int new_child_ref_id, int ref_child_ref_id), {
  try {
    const parent = globalThis.picorubyRefs[parent_ref_id];
    const newChild = globalThis.picorubyRefs[new_child_ref_id];
    const refChild = globalThis.picorubyRefs[ref_child_ref_id];
    if (!parent || !newChild) return false;
    parent.insertBefore(newChild, refChild);
    return true;
  } catch(e) {
    console.error('Error in js_insert_before:', e);
    return false;
  }
});

EM_JS(bool, js_set_attribute, (int ref_id, const char* name, const char* value), {
  try {
    const element = globalThis.picorubyRefs[ref_id];
    if (!element || !element.setAttribute) return false;
    element.setAttribute(UTF8ToString(name), UTF8ToString(value));
    return true;
  } catch(e) {
    console.error('Error in js_set_attribute:', e);
    return false;
  }
});

EM_JS(bool, js_remove_attribute, (int ref_id, const char* name), {
  try {
    const element = globalThis.picorubyRefs[ref_id];
    if (!element || !element.removeAttribute) return false;
    element.removeAttribute(UTF8ToString(name));
    return true;
  } catch(e) {
    console.error('Error in js_remove_attribute:', e);
    return false;
  }
});

EM_JS(int, setup_binary_handler, (int ref_id, uintptr_t mrb_ptr, uintptr_t task_ptr, uintptr_t callback_id), {
  const response = globalThis.picorubyRefs[ref_id];
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
 * Ruby <-> JavaScript value conversion functions
 *****************************************************/

/*
 * Convert Ruby value to JavaScript reference ID
 * Returns ref_id on success, -1 on error
 */
static int
ruby_value_to_js_ref(mrb_state *mrb, mrb_value value)
{
  if (mrb_nil_p(value)) {
    int ref_id = js_create_object();
    if (ref_id < 0) return -1;
    // Set to null explicitly
    EM_ASM_({ globalThis.picorubyRefs[$0] = null; }, ref_id);
    return ref_id;
  }

  if (mrb_true_p(value)) {
    int ref_id = js_create_object();
    if (ref_id < 0) return -1;
    EM_ASM_({ globalThis.picorubyRefs[$0] = true; }, ref_id);
    return ref_id;
  }

  if (mrb_false_p(value)) {
    int ref_id = js_create_object();
    if (ref_id < 0) return -1;
    EM_ASM_({ globalThis.picorubyRefs[$0] = false; }, ref_id);
    return ref_id;
  }

  if (mrb_integer_p(value)) {
    mrb_int num = mrb_integer(value);
    int ref_id = js_create_object();
    if (ref_id < 0) return -1;
    EM_ASM_({ globalThis.picorubyRefs[$0] = $1; }, ref_id, (int)num);
    return ref_id;
  }

  if (mrb_float_p(value)) {
    mrb_float num = mrb_float(value);
    int ref_id = js_create_object();
    if (ref_id < 0) return -1;
    EM_ASM_({ globalThis.picorubyRefs[$0] = $1; }, ref_id, num);
    return ref_id;
  }

  if (mrb_string_p(value)) {
    const char *str = RSTRING_PTR(value);
    int ref_id = js_create_object();
    if (ref_id < 0) return -1;
    EM_ASM_({
      const str = UTF8ToString($1);
      globalThis.picorubyRefs[$0] = str;
    }, ref_id, str);
    return ref_id;
  }

  if (mrb_array_p(value)) {
    int array_ref_id = js_create_array();
    if (array_ref_id < 0) return -1;

    mrb_int len = RARRAY_LEN(value);
    for (mrb_int i = 0; i < len; i++) {
      mrb_value elem = mrb_ary_ref(mrb, value, i);
      int elem_ref_id = ruby_value_to_js_ref(mrb, elem);
      if (elem_ref_id < 0) return -1;

      // Push element to JavaScript array, then delete the temporary ref
      EM_ASM_({
        const arr = globalThis.picorubyRefs[$0];
        const elem = globalThis.picorubyRefs[$1];
        arr.push(elem);
        delete globalThis.picorubyRefs[$1];
      }, array_ref_id, elem_ref_id);
    }
    return array_ref_id;
  }

  if (mrb_hash_p(value)) {
    int obj_ref_id = js_create_object();
    if (obj_ref_id < 0) return -1;

    mrb_value keys = mrb_hash_keys(mrb, value);
    mrb_int len = RARRAY_LEN(keys);
    for (mrb_int i = 0; i < len; i++) {
      mrb_value key = mrb_ary_ref(mrb, keys, i);
      mrb_value val = mrb_hash_get(mrb, value, key);

      const char *key_str;
      if (mrb_string_p(key)) {
        key_str = RSTRING_PTR(key);
      } else if (mrb_symbol_p(key)) {
        key_str = mrb_sym_name(mrb, mrb_symbol(key));
      } else {
        // Convert other types to string
        mrb_value key_s = mrb_funcall(mrb, key, "to_s", 0);
        key_str = RSTRING_PTR(key_s);
      }

      int val_ref_id = ruby_value_to_js_ref(mrb, val);
      if (val_ref_id < 0) return -1;

      EM_ASM_({
        const obj = globalThis.picorubyRefs[$0];
        const key = UTF8ToString($1);
        const val = globalThis.picorubyRefs[$2];
        obj[key] = val;
        delete globalThis.picorubyRefs[$2];
      }, obj_ref_id, key_str, val_ref_id);
    }
    return obj_ref_id;
  }

  // JS::Object - already has ref_id
  if (mrb_obj_is_kind_of(mrb, value, class_JS_Object)) {
    picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(value);
    return js_obj->ref_id;
  }

  // Unsupported type - convert to string
  mrb_value str = mrb_funcall(mrb, value, "to_s", 0);
  return ruby_value_to_js_ref(mrb, str);
}


/*****************************************************
 * callback functions
 *****************************************************/

inline static char*
callback_script(uintptr_t callback_id, int event_ref_id)
{
  static char script[512];
  snprintf(script, sizeof(script),
    // Use safe navigation operator (&.) to avoid errors if callback is removed
    // Chrome's autofill triggers events after removing listeners
    "JS::Object::CALLBACKS[%lu]&.call($js_events[%d])",
    callback_id, event_ref_id);
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

EMSCRIPTEN_KEEPALIVE
int
call_ruby_callback_sync_generic(uintptr_t callback_id, int *arg_ref_ids, int argc)
{
  if (!global_mrb) {
    return -1;
  }

  // Convert JavaScript arguments to Ruby array
  mrb_value args_array = mrb_ary_new_capa(global_mrb, argc);
  for (int i = 0; i < argc; i++) {
    int ref_id = arg_ref_ids[i];
    int js_type = get_js_type(ref_id);
    mrb_value arg_value;

    switch (js_type) {
      case 0: // NULL
      case 1: // UNDEFINED
        arg_value = mrb_nil_value();
        break;
      case 2: // BOOLEAN
        arg_value = get_boolean_value(ref_id) ? mrb_true_value() : mrb_false_value();
        break;
      case 3: // NUMBER
        {
          double num = get_number_value(ref_id);
          if (num == (int)num) {
            arg_value = mrb_fixnum_value((mrb_int)num);
          } else {
            arg_value = mrb_float_value(global_mrb, num);
          }
        }
        break;
      case 4: // STRING
        {
          int str_len = get_string_value_length(ref_id);
          char *buffer = (char *)mrb_malloc(global_mrb, str_len + 1);
          copy_string_value(ref_id, buffer, str_len + 1);
          arg_value = mrb_str_new_cstr(global_mrb, buffer);
          mrb_free(global_mrb, buffer);
        }
        break;
      case 5: // OBJECT
      default:
        {
          picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(global_mrb, sizeof(picorb_js_obj));
          data->ref_id = ref_id;
          arg_value = mrb_obj_value(Data_Wrap_Struct(global_mrb, class_JS_Object, &picorb_js_obj_type, data));
        }
        break;
    }
    mrb_ary_push(global_mrb, args_array, arg_value);
  }

  // Store arguments in global variable $js_callback_args
  mrb_value callback_args = mrb_gv_get(global_mrb, MRB_GVSYM(js_callback_args));
  if (mrb_nil_p(callback_args)) {
    callback_args = mrb_hash_new(global_mrb);
    mrb_gv_set(global_mrb, MRB_GVSYM(js_callback_args), callback_args);
  }
  int args_id = (int)callback_id;
  mrb_hash_set(global_mrb, callback_args, mrb_fixnum_value(args_id), args_array);

  // Generate script to call the callback
  static char script[512];
  snprintf(script, sizeof(script),
    "JS::Object::CALLBACKS[%lu]&.call(*$js_callback_args[%d])",
    callback_id, args_id);

  // Compile the script
  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  const uint8_t *script_ptr = (const uint8_t *)script;
  size_t size = strlen(script);
  mrc_irep *irep = mrc_load_string_cxt(cc, &script_ptr, size);

  if (!irep) {
    mrc_ccontext_free(cc);
    return -1;
  }

  // Create a Proc from the compiled irep
  mrc_resolve_intern(cc, irep);
  struct RProc *proc = mrb_proc_new(global_mrb, irep);
  proc->c = NULL;
  mrb_value proc_val = mrb_obj_value(proc);

  // Execute the proc synchronously (no additional arguments needed)
  mrb_value result = mrb_execute_proc_synchronously(global_mrb, proc_val, 0, NULL);

  // Clean up
  mrb_hash_delete_key(global_mrb, callback_args, mrb_fixnum_value(args_id));
  mrc_irep_free(cc, irep);
  mrc_ccontext_free(cc);

  // Handle exception
  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    fprintf(stderr, "Generic callback exception: %s\n", RSTRING_PTR(exc_str));
    global_mrb->exc = NULL;
    return -1;
  }

  // Convert result to JavaScript ref_id
  int result_ref_id = ruby_value_to_js_ref(global_mrb, result);
  return result_ref_id;
}


/*****************************************************
 * methods for JS::Object
 *****************************************************/

/*
 * Common function to get JS property and wrap as JS::Object
 */
static mrb_value
get_js_property(mrb_state *mrb, int parent_ref_id, const char* property_name)
{
  int ref_id = get_property(parent_ref_id, property_name);

  if (ref_id < 0) {
    return mrb_nil_value();
  }

  // Check the type of the JavaScript value
  int js_type = get_js_type(ref_id);

  switch (js_type) {
    case 0: // NULL
    case 1: // UNDEFINED
      return mrb_nil_value();

    case 2: // BOOLEAN
      return get_boolean_value(ref_id) ? mrb_true_value() : mrb_false_value();

    case 3: // NUMBER
      {
        double num = get_number_value(ref_id);
        // Check if it's an integer
        if (num == (int)num) {
          return mrb_fixnum_value((mrb_int)num);
        } else {
          return mrb_float_value(mrb, num);
        }
      }

    case 4: // STRING
      {
        int str_len = get_string_value_length(ref_id);
        char *buffer = (char *)mrb_malloc(mrb, str_len + 1);
        copy_string_value(ref_id, buffer, str_len + 1);
        mrb_value str = mrb_str_new_cstr(mrb, buffer);
        mrb_free(mrb, buffer);
        return str;
      }

    case 6: // FUNCTION
      // Functions should remain as JS::Object
      {
        picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
        data->ref_id = ref_id;
        mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
        return obj;
      }

    case 5: // OBJECT
    default:
      {
        // Check if it's an array-like object (has length property and numeric indices)
        // Note: We need to distinguish between strings (which have length) and array-like objects
        int length_ref_id = get_property(ref_id, "length");
        if (length_ref_id >= 0) {
          int length_type = get_js_type(length_ref_id);
          if (length_type == 3) { // NUMBER
            int length = (int)get_number_value(length_ref_id);
            // Convert to Ruby Array
            mrb_value array = mrb_ary_new_capa(mrb, length);
            for (int i = 0; i < length; i++) {
              int element_ref_id = get_element(ref_id, i);
              if (element_ref_id < 0) {
                mrb_ary_push(mrb, array, mrb_nil_value());
                continue;
              }
              // Recursively convert element
              int element_type = get_js_type(element_ref_id);
              mrb_value element;
              switch (element_type) {
                case 0: // NULL
                case 1: // UNDEFINED
                  element = mrb_nil_value();
                  break;
                case 2: // BOOLEAN
                  element = get_boolean_value(element_ref_id) ? mrb_true_value() : mrb_false_value();
                  break;
                case 3: // NUMBER
                  {
                    double num = get_number_value(element_ref_id);
                    if (num == (int)num) {
                      element = mrb_fixnum_value((mrb_int)num);
                    } else {
                      element = mrb_float_value(mrb, num);
                    }
                  }
                  break;
                case 4: // STRING
                  {
                    int str_len = get_string_value_length(element_ref_id);
                    char *buffer = (char *)mrb_malloc(mrb, str_len + 1);
                    copy_string_value(element_ref_id, buffer, str_len + 1);
                    element = mrb_str_new_cstr(mrb, buffer);
                    mrb_free(mrb, buffer);
                  }
                  break;
                default: // OBJECT or FUNCTION - wrap as JS::Object
                  {
                    picorb_js_obj *elem_data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
                    elem_data->ref_id = element_ref_id;
                    element = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, elem_data));
                  }
                  break;
              }
              mrb_ary_push(mrb, array, element);
            }
            return array;
          }
        }

        // Not array-like, return as JS::Object
        picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
        data->ref_id = ref_id;
        mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
        return obj;
      }
  }
}

/*
 * JS::Object#[]
 */
static mrb_value
mrb_object_get_property(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *parent = (picorb_js_obj *)DATA_PTR(self);
  mrb_value key;
  mrb_get_args(mrb, "o", &key);

  const char* key_str;
  char int_buf[32];

  if (mrb_integer_p(key)) {
    // Convert integer to string
    snprintf(int_buf, sizeof(int_buf), "%lld", (long long)mrb_integer(key));
    key_str = int_buf;
  } else if (mrb_symbol_p(key)) {
    key_str = mrb_sym_name(mrb, mrb_symbol(key));
  } else if (mrb_string_p(key)) {
    key_str = RSTRING_PTR(key);
  } else {
    mrb_raisef(mrb, E_TYPE_ERROR, "%v is not a symbol nor a string", key);
  }

  return get_js_property(mrb, parent->ref_id, key_str);
}

/*
 * Common function to set JS property
 */
static bool
set_js_property(mrb_state *mrb, int ref_id, const char* property_name, mrb_value value)
{
  bool success = false;
  if (mrb_string_p(value)) {
    success = set_property(ref_id, property_name, RSTRING_PTR(value));
  } else if (mrb_integer_p(value)) {
    success = set_property_int(ref_id, property_name, mrb_integer(value));
  } else if (mrb_float_p(value)) {
    success = set_property_double(ref_id, property_name, mrb_float(value));
  } else if (mrb_true_p(value) || mrb_false_p(value)) {
    success = set_property_bool(ref_id, property_name, mrb_bool(value));
  } else if (mrb_nil_p(value)) {
    success = set_property_null(ref_id, property_name);
  } else if (mrb_obj_is_kind_of(mrb, value, class_JS_Object)) {
    picorb_js_obj *value_obj = (picorb_js_obj *)DATA_PTR(value);
    success = set_property_ref(ref_id, property_name, value_obj->ref_id);
  } else {
    mrb_raisef(mrb, E_TYPE_ERROR, "unsupported type for property value: %T", value);
  }
  return success;
}

/*
 * JS::Object#[]=
 */
static mrb_value
mrb_object_set_property(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(self);
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "oo", &key, &value);

  const char* key_str;
  char int_buf[32];

  if (mrb_integer_p(key)) {
    // Convert integer to string
    snprintf(int_buf, sizeof(int_buf), "%lld", (long long)mrb_integer(key));
    key_str = int_buf;
  } else if (mrb_symbol_p(key)) {
    key_str = mrb_sym_name(mrb, mrb_symbol(key));
  } else if (mrb_string_p(key)) {
    key_str = RSTRING_PTR(key);
  } else {
    mrb_raisef(mrb, E_TYPE_ERROR, "%v is not a symbol nor a string", key);
  }

  bool success = set_js_property(mrb, js_obj->ref_id, key_str, value);
  if (!success) {
    return mrb_nil_value();
  }
  return value;
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
 * JS::Object#type for debug
 */
static mrb_value
mrb_object_type(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  js_type_info info = {};
  js_get_type_info(obj->ref_id, &info);
  mrb_value type_sym;
  switch (info.type) {
    case TYPE_UNDEFINED:
      type_sym = mrb_symbol_value(MRB_SYM(undefined));
      break;
    case TYPE_NULL:
      type_sym = mrb_symbol_value(MRB_SYM(null));
      break;
    case TYPE_BOOLEAN:
      type_sym = mrb_symbol_value(MRB_SYM(boolean));
      break;
    case TYPE_NUMBER:
      type_sym = mrb_symbol_value(MRB_SYM(number));
      break;
    case TYPE_BIGINT:
      type_sym = mrb_symbol_value(MRB_SYM(bigint));
      break;
    case TYPE_STRING:
      type_sym = mrb_symbol_value(MRB_SYM(string));
      break;
    case TYPE_SYMBOL:
      type_sym = mrb_symbol_value(MRB_SYM(symbol));
      break;
    case TYPE_ARRAY:
      type_sym = mrb_symbol_value(MRB_SYM(array));
      break;
    case TYPE_OBJECT:
      type_sym = mrb_symbol_value(MRB_SYM(object));
      break;
    case TYPE_FUNCTION:
      type_sym = mrb_symbol_value(MRB_SYM(function));
      break;
    default:
      type_sym = mrb_symbol_value(MRB_SYM(unknown));
      break;
  }
  return type_sym;
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

    bool success = set_js_property(mrb, js_obj->ref_id, property_name, argv[0]);
    if (!success) {
      return mrb_nil_value();
    }
    return argv[0];
  }

  int new_ref_id;

  if (argc == 0) { // No argument
    // Check if property is a function
    int js_type = get_js_property_type(js_obj->ref_id, method_name);
    if (js_type == 6) { // FUNCTION type
      // Call as method
      call_method_no_return(js_obj->ref_id, method_name);
      return mrb_nil_value();
    }
    // Try to get property
    mrb_value property_obj = get_js_property(mrb, js_obj->ref_id, method_name);
    if (!mrb_nil_p(property_obj)) {
      return property_obj;
    }
    // If property doesn't exist, try calling as method
    new_ref_id = call_method(js_obj->ref_id, method_name, "");
    if (new_ref_id < 0) {
      return mrb_nil_value();
    }
  } else if (argc == 1) { // One argument
    int new_ref_id = -1;
    if (mrb_string_p(argv[0])) {
      new_ref_id = call_method(js_obj->ref_id, method_name, RSTRING_PTR(argv[0]));
    } else if (mrb_integer_p(argv[0])) {
      new_ref_id = call_method_int(js_obj->ref_id, method_name, mrb_integer(argv[0]));
    } else if (mrb_obj_is_kind_of(mrb, argv[0], class_JS_Object)) {
      picorb_js_obj *arg_obj = (picorb_js_obj *)DATA_PTR(argv[0]);
      new_ref_id = call_method_with_ref(js_obj->ref_id, method_name, arg_obj->ref_id);
    } else {
      mrb_raise(mrb, E_TYPE_ERROR, "argument must be a String, Integer, or JS::Object");
      return mrb_nil_value();
    }

    if (new_ref_id < 0) {
      return mrb_nil_value();
    }

    // Apply automatic type conversion based on JavaScript type
    int js_type = get_js_type(new_ref_id);
    switch (js_type) {
      case 0: // NULL
        return mrb_nil_value();
      case 1: // UNDEFINED
        return mrb_nil_value();
      case 2: // BOOLEAN
        {
          bool value = get_boolean_value(new_ref_id);
          return mrb_bool_value(value);
        }
      case 3: // NUMBER
        {
          double value = get_number_value(new_ref_id);
          return mrb_float_value(mrb, value);
        }
      case 4: // STRING
        {
          int str_len = get_string_value_length(new_ref_id);
          char *buffer = (char *)mrb_malloc(mrb, str_len + 1);
          copy_string_value(new_ref_id, buffer, str_len + 1);
          mrb_value result = mrb_str_new_cstr(mrb, buffer);
          mrb_free(mrb, buffer);
          return result;
        }
      case 5: // OBJECT
      default:
        {
          // Check if it's an array-like object (has length property)
          int length_ref_id = get_property(new_ref_id, "length");
          if (length_ref_id >= 0) {
            int length_type = get_js_type(length_ref_id);
            if (length_type == 3) { // NUMBER
              int length = (int)get_number_value(length_ref_id);

              // Check if it's a string (strings also have length)
              js_type = get_js_type(new_ref_id);
              if (js_type == 4) { // STRING
                int str_len = get_string_value_length(new_ref_id);
                char *buffer = (char *)mrb_malloc(mrb, str_len + 1);
                copy_string_value(new_ref_id, buffer, str_len + 1);
                mrb_value result = mrb_str_new_cstr(mrb, buffer);
                mrb_free(mrb, buffer);
                return result;
              }

              // Convert to Ruby Array
              mrb_value array = mrb_ary_new_capa(mrb, length);
              for (int i = 0; i < length; i++) {
                int item_ref_id = get_element(new_ref_id, i);
                if (item_ref_id < 0) {
                  mrb_ary_push(mrb, array, mrb_nil_value());
                  continue;
                }
                int item_type = get_js_type(item_ref_id);
                switch (item_type) {
                  case 0: // NULL
                  case 1: // UNDEFINED
                    mrb_ary_push(mrb, array, mrb_nil_value());
                    break;
                  case 2: // BOOLEAN
                    {
                      bool value = get_boolean_value(item_ref_id);
                      mrb_ary_push(mrb, array, mrb_bool_value(value));
                    }
                    break;
                  case 3: // NUMBER
                    {
                      double value = get_number_value(item_ref_id);
                      mrb_ary_push(mrb, array, mrb_float_value(mrb, value));
                    }
                    break;
                  case 4: // STRING
                    {
                      int str_len = get_string_value_length(item_ref_id);
                      char *buffer = (char *)mrb_malloc(mrb, str_len + 1);
                      copy_string_value(item_ref_id, buffer, str_len + 1);
                      mrb_ary_push(mrb, array, mrb_str_new_cstr(mrb, buffer));
                      mrb_free(mrb, buffer);
                    }
                    break;
                  case 5: // OBJECT
                  case 6: // FUNCTION
                  default:
                    {
                      picorb_js_obj *item_data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
                      item_data->ref_id = item_ref_id;
                      mrb_value item_obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, item_data));
                      mrb_ary_push(mrb, array, item_obj);
                    }
                    break;
                }
              }
              return array;
            }
          }
          // Not array-like, return as JS::Object
          picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
          data->ref_id = new_ref_id;
          mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
          return obj;
        }
    }
  } else if (argc == 2){
    int new_ref_id = -1;
    if (mrb_obj_is_kind_of(mrb, argv[0], class_JS_Object) && mrb_obj_is_kind_of(mrb, argv[1], class_JS_Object)) {
      picorb_js_obj *arg_obj_1 = (picorb_js_obj *)DATA_PTR(argv[0]);
      picorb_js_obj *arg_obj_2 = (picorb_js_obj *)DATA_PTR(argv[1]);
      new_ref_id = call_method_with_ref_ref(js_obj->ref_id, method_name, arg_obj_1->ref_id, arg_obj_2->ref_id);
    } else if (mrb_string_p(argv[0]) && mrb_string_p(argv[1])) {
      new_ref_id = call_method_str(js_obj->ref_id, method_name, RSTRING_PTR(argv[0]), RSTRING_PTR(argv[1]));
    } else {
      mrb_raisef(mrb, E_TYPE_ERROR, "method: %s, argc: %d", method_name, argc);
      return mrb_nil_value();
    }

    if (new_ref_id < 0) {
      return mrb_nil_value();
    }

    // Apply automatic type conversion based on JavaScript type
    int js_type = get_js_type(new_ref_id);
    switch (js_type) {
      case 0: // NULL
        return mrb_nil_value();
      case 1: // UNDEFINED
        return mrb_nil_value();
      case 2: // BOOLEAN
        {
          bool value = get_boolean_value(new_ref_id);
          return mrb_bool_value(value);
        }
      case 3: // NUMBER
        {
          double value = get_number_value(new_ref_id);
          return mrb_float_value(mrb, value);
        }
      case 4: // STRING
        {
          int str_len = get_string_value_length(new_ref_id);
          char *buffer = (char *)mrb_malloc(mrb, str_len + 1);
          copy_string_value(new_ref_id, buffer, str_len + 1);
          mrb_value result = mrb_str_new_cstr(mrb, buffer);
          mrb_free(mrb, buffer);
          return result;
        }
      case 5: // OBJECT
      default:
        // Return as JS::Object (no array-like conversion for 2-arg methods)
        {
          picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
          data->ref_id = new_ref_id;
          mrb_value obj = mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
          return obj;
        }
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
 * JS::Object.register_callback
 * Register a Ruby block as a callable JavaScript function
 */
static mrb_value
mrb_object_s__register_callback(mrb_state *mrb, mrb_value self)
{
  mrb_int callback_id;
  char *callback_name;
  mrb_get_args(mrb, "iz", &callback_id, &callback_name);
  js_register_generic_callback((uintptr_t)callback_id, callback_name);
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
 * JS::Object#_fetch_with_options_and_suspend
 */
static mrb_value
mrb_object__fetch_with_options_and_suspend(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  char *url;
  char *options_json;
  mrb_int callback_id;
  mrb_get_args(mrb, "zzi", &url, &options_json, &callback_id);
  int promise_id = call_fetch_with_json_options(obj->ref_id, url, options_json);

  mrb_value current_task = mrb_funcall_id(mrb, mrb_obj_value(mrb_class_get_id(mrb, MRB_SYM(Task))), MRB_SYM(current), 0);

  uintptr_t task_ptr = (uintptr_t)mrb_val_union(current_task).vp;

  mrb_suspend_task(mrb, current_task);
  setup_promise_handler(promise_id, (uintptr_t)callback_id, (uintptr_t)mrb, task_ptr);

  return mrb_nil_value();
}

/*
 * JS::Object#_removeEventListener
 */
static mrb_value
mrb_object__remove_event_listener(mrb_state *mrb, mrb_value self)
{
  mrb_int callback_id;
  mrb_get_args(mrb, "i", &callback_id);
  bool success = js_remove_event_listener((uintptr_t)callback_id);
  return mrb_bool_value(success);
}

/*
 * JS::Object#createElement
 */
static mrb_value
mrb_object_create_element(mrb_state *mrb, mrb_value self)
{
  char *tag_name;
  mrb_get_args(mrb, "z", &tag_name);
  int ref_id = js_create_element(tag_name);
  if (ref_id < 0) {
    return mrb_nil_value();
  }
  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
  data->ref_id = ref_id;
  return mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
}

/*
 * JS::Object#createTextNode
 */
static mrb_value
mrb_object_create_text_node(mrb_state *mrb, mrb_value self)
{
  char *text;
  mrb_get_args(mrb, "z", &text);
  int ref_id = js_create_text_node(text);
  if (ref_id < 0) {
    return mrb_nil_value();
  }
  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
  data->ref_id = ref_id;
  return mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
}

/*
 * JS::Object#_create_object
 * Create a new JavaScript object {}
 */
static mrb_value
mrb_object_create_object(mrb_state *mrb, mrb_value self)
{
  mrb_get_args(mrb, "");
  int ref_id = js_create_object();
  if (ref_id < 0) {
    return mrb_nil_value();
  }
  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
  data->ref_id = ref_id;
  return mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
}

/*
 * JS::Object#_create_array
 * Create a new JavaScript array []
 */
static mrb_value
mrb_object_create_array(mrb_state *mrb, mrb_value self)
{
  mrb_get_args(mrb, "");
  int ref_id = js_create_array();
  if (ref_id < 0) {
    return mrb_nil_value();
  }
  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
  data->ref_id = ref_id;
  return mrb_obj_value(Data_Wrap_Struct(mrb, class_JS_Object, &picorb_js_obj_type, data));
}

/*
 * JS::Object#appendChild
 */
static mrb_value
mrb_object_append_child(mrb_state *mrb, mrb_value self)
{
  mrb_value child;
  mrb_get_args(mrb, "o", &child);
  if (!mrb_obj_is_kind_of(mrb, child, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "argument must be JS::Object");
    return mrb_nil_value();
  }
  picorb_js_obj *parent_obj = (picorb_js_obj *)DATA_PTR(self);
  picorb_js_obj *child_obj = (picorb_js_obj *)DATA_PTR(child);
  bool success = js_append_child(parent_obj->ref_id, child_obj->ref_id);
  return mrb_bool_value(success);
}

/*
 * JS::Object#removeChild
 */
static mrb_value
mrb_object_remove_child(mrb_state *mrb, mrb_value self)
{
  mrb_value child;
  mrb_get_args(mrb, "o", &child);
  if (!mrb_obj_is_kind_of(mrb, child, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "argument must be JS::Object");
    return mrb_nil_value();
  }
  picorb_js_obj *parent_obj = (picorb_js_obj *)DATA_PTR(self);
  picorb_js_obj *child_obj = (picorb_js_obj *)DATA_PTR(child);
  bool success = js_remove_child(parent_obj->ref_id, child_obj->ref_id);
  return mrb_bool_value(success);
}

/*
 * JS::Object#replaceChild
 */
static mrb_value
mrb_object_replace_child(mrb_state *mrb, mrb_value self)
{
  mrb_value new_child, old_child;
  mrb_get_args(mrb, "oo", &new_child, &old_child);
  if (!mrb_obj_is_kind_of(mrb, new_child, class_JS_Object) ||
      !mrb_obj_is_kind_of(mrb, old_child, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "arguments must be JS::Object");
    return mrb_nil_value();
  }
  picorb_js_obj *parent_obj = (picorb_js_obj *)DATA_PTR(self);
  picorb_js_obj *new_child_obj = (picorb_js_obj *)DATA_PTR(new_child);
  picorb_js_obj *old_child_obj = (picorb_js_obj *)DATA_PTR(old_child);
  bool success = js_replace_child(parent_obj->ref_id, new_child_obj->ref_id, old_child_obj->ref_id);
  return mrb_bool_value(success);
}

/*
 * JS::Object#insertBefore
 */
static mrb_value
mrb_object_insert_before(mrb_state *mrb, mrb_value self)
{
  mrb_value new_child, ref_child;
  mrb_get_args(mrb, "oo", &new_child, &ref_child);
  if (!mrb_obj_is_kind_of(mrb, new_child, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "new_child must be JS::Object");
    return mrb_nil_value();
  }
  picorb_js_obj *parent_obj = (picorb_js_obj *)DATA_PTR(self);
  picorb_js_obj *new_child_obj = (picorb_js_obj *)DATA_PTR(new_child);
  int ref_child_ref_id = mrb_nil_p(ref_child) ? -1 :
    ((picorb_js_obj *)DATA_PTR(ref_child))->ref_id;
  bool success = js_insert_before(parent_obj->ref_id, new_child_obj->ref_id, ref_child_ref_id);
  return mrb_bool_value(success);
}

/*
 * JS::Object#setAttribute
 */
static mrb_value
mrb_object_set_attribute(mrb_state *mrb, mrb_value self)
{
  char *name, *value;
  mrb_get_args(mrb, "zz", &name, &value);
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  bool success = js_set_attribute(obj->ref_id, name, value);
  return mrb_bool_value(success);
}

/*
 * JS::Object#removeAttribute
 */
static mrb_value
mrb_object_remove_attribute(mrb_state *mrb, mrb_value self)
{
  char *name;
  mrb_get_args(mrb, "z", &name);
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  bool success = js_remove_attribute(obj->ref_id, name);
  return mrb_bool_value(success);
}

/*
 * JS::Object#_preventDefault
 */
static mrb_value
mrb_object_prevent_default(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  call_method_no_return(obj->ref_id, "preventDefault");
  return mrb_nil_value();
}

/*
 * JS::Object#_stopPropagation
 */
static mrb_value
mrb_object_stop_propagation(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  call_method_no_return(obj->ref_id, "stopPropagation");
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
  mrb_define_method_id(mrb, class_JS_Object, MRB_OPSYM(aset), mrb_object_set_property, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(method_missing), mrb_object_method_missing, MRB_ARGS_ANY());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_add_event_listener), mrb_object__add_event_listener, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_JS_Object, MRB_SYM(_register_callback), mrb_object_s__register_callback, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_fetch_and_suspend), mrb_object__fetch_and_suspend, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_fetch_with_options_and_suspend), mrb_object__fetch_with_options_and_suspend, MRB_ARGS_REQ(3));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_to_binary_and_suspend), mrb_object__to_binary_and_suspend, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(type), mrb_object_type, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(refcount), mrb_js_refcount, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_removeEventListener), mrb_object__remove_event_listener, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(createElement), mrb_object_create_element, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(createTextNode), mrb_object_create_text_node, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(create_object), mrb_object_create_object, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(create_array), mrb_object_create_array, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(appendChild), mrb_object_append_child, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(removeChild), mrb_object_remove_child, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(replaceChild), mrb_object_replace_child, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(insertBefore), mrb_object_insert_before, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(setAttribute), mrb_object_set_attribute, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(removeAttribute), mrb_object_remove_attribute, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(preventDefault), mrb_object_prevent_default, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(stopPropagation), mrb_object_stop_propagation, MRB_ARGS_NONE());
}
