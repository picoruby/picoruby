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
#include "mruby/proc.h"

#include "mruby_compiler.h"
#include "mrc_utils.h"
#include "task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct picorb_js_obj {
  int ref_id;
} picorb_js_obj;

typedef enum {
  JS_TYPE_UNDEFINED = 0,
  JS_TYPE_NULL = 1,
  JS_TYPE_BOOLEAN = 2,
  JS_TYPE_NUMBER = 3,
  JS_TYPE_BIGINT = 4,
  JS_TYPE_STRING = 5,
  JS_TYPE_SYMBOL = 6,
  JS_TYPE_ARRAY = 7,
  JS_TYPE_OBJECT = 8,
  JS_TYPE_FUNCTION = 9
} js_value_type;

struct RClass *class_JS_Object;
struct RClass *class_JS_Array;
struct RClass *class_JS_Function;
struct RClass *class_JS_Promise;
struct RClass *class_JS_Event;
struct RClass *class_JS_Response;
struct RClass *class_JS_Element;

typedef enum {
  JS_COMPOSITE_PLAIN = 0,
  JS_COMPOSITE_ARRAY = 1,
  JS_COMPOSITE_FUNCTION = 2,
  JS_COMPOSITE_PROMISE = 3
} js_composite_kind;

typedef enum {
  JS_DOM_PLAIN = 0,
  JS_DOM_EVENT = 1,
  JS_DOM_RESPONSE = 2,
  JS_DOM_ELEMENT = 3
} js_dom_kind;

static void
picorb_js_obj_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

const struct mrb_data_type picorb_js_obj_type = {
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

EM_JS(bool, set_property, (int ref_id, const char* key, const char* value, int value_len), {
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
    const val = UTF8ToString(value, value_len);
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

    if (value === null) return 1; // TYPE_NULL
    if (value === undefined) return 0; // TYPE_UNDEFINED
    if (type === 'boolean') return 2; // TYPE_BOOLEAN
    if (type === 'number') return 3; // TYPE_NUMBER
    if (type === 'string') return 5; // TYPE_STRING
    // Add check for String objects before Array.isArray
    if (value instanceof String) return 5; // TYPE_STRING object, treat as primitive string
    if (Array.isArray(value)) return 7;  // TYPE_ARRAY
    if (type === 'function') return 9; // TYPE_FUNCTION
    return 8; // TYPE_OBJECT
  } catch(e) {
    console.error('Error in get_js_type (JS side):', e);
    return 0; // TYPE_UNDEFINED
  }
});

EM_JS(int, get_js_property_type, (int ref_id, const char* property_name), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const propName = UTF8ToString(property_name);
    const value = obj[propName];
    const type = typeof value;

    if (value === null) return 1; // JS_TYPE_NULL
    if (value === undefined) return 0; // JS_TYPE_UNDEFINED
    if (type === 'boolean') return 2; // JS_TYPE_BOOLEAN
    if (type === 'number') return 3; // JS_TYPE_NUMBER
    if (type === 'string') return 5; // JS_TYPE_STRING
    if (value instanceof String) return 5; // JS_TYPE_STRING object
    if (Array.isArray(value)) return 7; // JS_TYPE_ARRAY
    if (type === 'function') return 9; // JS_TYPE_FUNCTION
    return 8; // JS_TYPE_OBJECT
  } catch(e) {
    return 0; // JS_TYPE_UNDEFINED
  }
});

EM_JS(bool, get_boolean_value, (int ref_id), {
  return globalThis.picorubyRefs[ref_id] ? true : false;
});

// Classify a composite JS value into an enum used to pick the Ruby class
// (JS::Object / JS::Array / JS::Function / JS::Promise) when wrapping.
// Only meaningful for values that get_js_type returned as composite.
EM_JS(int, js_classify_composite, (int ref_id), {
  try {
    const v = globalThis.picorubyRefs[ref_id];
    if (Array.isArray(v)) return 1;
    if (typeof v === 'function') return 2;
    if (typeof Promise !== 'undefined' && v instanceof Promise) return 3;
    return 0;
  } catch(e) {
    return 0;
  }
});

// Classify a plain-object JS value into a DOM-related Ruby subclass.
// Document is treated as Element-like so that doc.createElement /
// doc.appendChild dispatch to JS::Element methods (Document is a Node but
// not technically an Element in the DOM standard).
EM_JS(int, js_classify_dom, (int ref_id), {
  try {
    const v = globalThis.picorubyRefs[ref_id];
    if (typeof Event !== 'undefined' && v instanceof Event) return 1;
    if (typeof Response !== 'undefined' && v instanceof Response) return 2;
    if (typeof Element !== 'undefined' && (v instanceof Element ||
        (typeof Document !== 'undefined' && v instanceof Document))) return 3;
    return 0;
  } catch(e) {
    return 0;
  }
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

EM_JS(int, call_method, (int ref_id, const char* method, const char* arg, int arg_len), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];
    const argString = UTF8ToString(arg, arg_len);

    let result;
    if (methodName === 'new') {
      // Call as constructor
      result = new obj(argString);
    } else {
      // Call as method
      if (typeof func !== 'function') {
        console.error('Method not found or not a function:', methodName);
        return -1;
      }
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

EM_JS(int, call_method_no_arg, (int ref_id, const char* method), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];
    if (typeof func !== 'function') {
      console.error('Method not found or not a function:', methodName);
      return -1;
    }
    let result = func.call(obj);
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
      if (typeof func !== 'function') {
        console.error('Method not found or not a function:', methodName);
        return -1;
      }
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

EM_JS(int, call_method_str, (int ref_id, const char* method, const char* arg1, int arg1_len, const char *arg2, int arg2_len), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];
    const argString1 = UTF8ToString(arg1, arg1_len);
    const argString2 = UTF8ToString(arg2, arg2_len);

    let result;
    if (methodName === 'new') {
      // Call as constructor
      result = new obj(argString1, argString2);
    } else {
      // Call as method
      if (typeof func !== 'function') {
        console.error('Method not found or not a function:', methodName);
        return -1;
      }
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
      if (typeof func !== 'function') {
        console.error('Method not found or not a function:', methodName);
        return -1;
      }
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
      if (typeof func !== 'function') {
        console.error('Method not found or not a function:', methodName);
        return -1;
      }
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

EM_JS(int, call_method_with_ref_str_str, (int ref_id, const char* method, int arg1_ref_id, const char* arg2_str, int arg2_len, const char* arg3_str, int arg3_len), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];

    const argObj1 = globalThis.picorubyRefs[arg1_ref_id];
    const argString2 = UTF8ToString(arg2_str, arg2_len);
    const argString3 = UTF8ToString(arg3_str, arg3_len);

    if (typeof func !== 'function') {
      console.error('Method not found or not a function:', methodName);
      return -1;
    }

    let result = func.call(obj, argObj1, argString2, argString3); // assuming not a constructor

    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error('Error in call_method_with_ref_str_str:', e);
    return -1;
  }
});

EM_JS(int, call_method_with_args, (int ref_id, const char* method, const char* args_json, int args_json_len), {
  try {
    const obj = globalThis.picorubyRefs[ref_id];
    const methodName = UTF8ToString(method);
    const func = obj[methodName];

    if (typeof func !== 'function') {
      console.error('Method not found or not a function:', methodName);
      return -1;
    }

    const argsStr = UTF8ToString(args_json, args_json_len);
    const argsData = JSON.parse(argsStr);

    if (!Array.isArray(argsData)) {
      console.error('args_json must be a JSON array');
      return -1;
    }

    // Convert type-tagged arguments to actual JavaScript values
    const args = argsData.map(arg => {
      switch (arg.type) {
        case 'string':
          return arg.value;
        case 'integer':
          return arg.value;
        case 'float':
          return arg.value;
        case 'boolean':
          return arg.value;
        case 'ref':
          return globalThis.picorubyRefs[arg.value];
        case 'nil':
          return null;
        default:
          console.error('Unknown argument type:', arg.type);
          return null;
      }
    });

    const result = func.call(obj, ...args);

    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error('Error in call_method_with_args:', e);
    return -1;
  }
});

EM_JS(int, call_constructor_with_args, (int ref_id, const char* args_json, int args_json_len), {
  try {
    const ctor = globalThis.picorubyRefs[ref_id];
    if (typeof ctor !== 'function') {
      console.error('Object is not a constructor function');
      return -1;
    }

    const argsStr = UTF8ToString(args_json, args_json_len);
    const argsData = JSON.parse(argsStr);

    if (!Array.isArray(argsData)) {
      console.error('args_json must be a JSON array');
      return -1;
    }

    const args = argsData.map(arg => {
      switch (arg.type) {
        case 'string':
          return arg.value;
        case 'integer':
          return arg.value;
        case 'float':
          return arg.value;
        case 'boolean':
          return arg.value;
        case 'ref':
          return globalThis.picorubyRefs[arg.value];
        case 'nil':
          return null;
        default:
          console.error('Unknown argument type:', arg.type);
          return null;
      }
    });

    const result = new ctor(...args);
    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error('Error in call_constructor_with_args:', e);
    return -1;
  }
});

// Invoke a JS function value directly (no `this` binding). The caller passes
// arguments as a JSON array encoded with the same tagged-value format used by
// call_method_with_args. Used by JS::Function#call.
EM_JS(int, js_function_apply_args, (int func_ref_id, const char* args_json, int args_json_len), {
  try {
    const fn = globalThis.picorubyRefs[func_ref_id];
    if (typeof fn !== 'function') {
      console.error('js_function_apply_args: not a function');
      return -1;
    }
    const argsData = JSON.parse(UTF8ToString(args_json, args_json_len));
    if (!Array.isArray(argsData)) {
      console.error('js_function_apply_args: args_json must be a JSON array');
      return -1;
    }
    const args = argsData.map(arg => {
      switch (arg.type) {
        case 'string':
        case 'integer':
        case 'float':
        case 'boolean':
          return arg.value;
        case 'ref':
          return globalThis.picorubyRefs[arg.value];
        case 'nil':
          return null;
        default:
          return null;
      }
    });
    const result = fn(...args);
    const newRefId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return newRefId;
  } catch(e) {
    console.error('Error in js_function_apply_args:', e);
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
    // Prevent default for submit events and click events on <a> tags
    // This allows Ruby event handlers to work as SPA navigation handlers
    if (type === 'submit' || (type === 'click' && target.tagName === 'A')) {
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

EM_JS(int, js_create_callback_function, (uintptr_t callback_id), {
  try {
    const fn = function(...args) {
      const argRefIds = args.map(arg => globalThis.picorubyRefs.push(arg) - 1);
      const argRefIdsPtr = _malloc(argRefIds.length * 4);
      for (let i = 0; i < argRefIds.length; i++) {
        HEAP32[(argRefIdsPtr >> 2) + i] = argRefIds[i];
      }

      const resultRefId = ccall(
        'call_ruby_callback_sync_generic',
        'number',
        ['number', 'number', 'number'],
        [callback_id, argRefIdsPtr, argRefIds.length]
      );

      _free(argRefIdsPtr);

      if (resultRefId >= 0 && resultRefId < globalThis.picorubyRefs.length) {
        return globalThis.picorubyRefs[resultRefId];
      }
      return undefined;
    };
    return globalThis.picorubyRefs.push(fn) - 1;
  } catch (e) {
    console.error('js_create_callback_function failed:', e);
    return -1;
  }
});

EM_JS(int, js_set_timeout, (uintptr_t callback_id, int delay_ms), {
  const timerId = setTimeout(function() {
    // Check if callback still exists before calling
    if (!globalThis.picorubyTimeoutHandlers || !globalThis.picorubyTimeoutHandlers[callback_id]) {
      return;
    }
    ccall(
      'call_ruby_callback',
      'void',
      ['number', 'number'],
      [callback_id, -1]  // -1 means no event object
    );
    // Clean up after one-shot timer
    delete globalThis.picorubyTimeoutHandlers[callback_id];
  }, delay_ms);

  // Store timer info for cleanup
  if (!globalThis.picorubyTimeoutHandlers) {
    globalThis.picorubyTimeoutHandlers = {};
  }
  globalThis.picorubyTimeoutHandlers[callback_id] = timerId;

  return timerId;
});

EM_JS(bool, js_clear_timeout, (uintptr_t callback_id), {
  try {
    if (!globalThis.picorubyTimeoutHandlers) return false;
    const timerId = globalThis.picorubyTimeoutHandlers[callback_id];
    if (timerId === undefined) return false;
    clearTimeout(timerId);
    delete globalThis.picorubyTimeoutHandlers[callback_id];
    return true;
  } catch(e) {
    console.error('Error in js_clear_timeout:', e);
    return false;
  }
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
 * Helper: wrap ref_id as the most specific JS::* subclass.
 * Picks JS::Array / JS::Function / JS::Promise for array-like / function /
 * Promise values; for plain objects, picks JS::Event / JS::Response /
 * JS::Element when the underlying value is a DOM Event / Response / Element
 * (or Document, treated as Element-like). Falls back to JS::Object.
 */
mrb_value
wrap_ref_as_js_object(mrb_state *mrb, int ref_id)
{
  struct RClass *klass = class_JS_Object;
  switch (js_classify_composite(ref_id)) {
    case JS_COMPOSITE_ARRAY:    klass = class_JS_Array; break;
    case JS_COMPOSITE_FUNCTION: klass = class_JS_Function; break;
    case JS_COMPOSITE_PROMISE:  klass = class_JS_Promise; break;
    case JS_COMPOSITE_PLAIN:
      switch (js_classify_dom(ref_id)) {
        case JS_DOM_EVENT:    klass = class_JS_Event; break;
        case JS_DOM_RESPONSE: klass = class_JS_Response; break;
        case JS_DOM_ELEMENT:  klass = class_JS_Element; break;
        default: break;
      }
      break;
  }
  picorb_js_obj *data = (picorb_js_obj *)mrb_malloc(mrb, sizeof(picorb_js_obj));
  data->ref_id = ref_id;
  return mrb_obj_value(Data_Wrap_Struct(mrb, klass, &picorb_js_obj_type, data));
}

/*
 * Helper: convert ref_id to Ruby value based on JS type.
 *
 * Primitive JS values (string / number / boolean / null / undefined) are
 * unwrapped to Ruby native values (String / Integer / Float / true / false / nil).
 * Composite values (object / array / function / symbol / bigint) are wrapped
 * as JS::Object.
 */
static mrb_value
js_ref_to_ruby_value(mrb_state *mrb, int ref_id)
{
  if (ref_id < 0) {
    return mrb_nil_value();
  }
  int js_type = get_js_type(ref_id);
  switch (js_type) {
    case JS_TYPE_UNDEFINED:
    case JS_TYPE_NULL:
      return mrb_nil_value();
    case JS_TYPE_BOOLEAN:
      return get_boolean_value(ref_id) ? mrb_true_value() : mrb_false_value();
    case JS_TYPE_NUMBER: {
      double num = get_number_value(ref_id);
      if (num == (double)(mrb_int)num) {
        return mrb_fixnum_value((mrb_int)num);
      }
      return mrb_float_value(mrb, num);
    }
    case JS_TYPE_STRING: {
      int str_len = get_string_value_length(ref_id);
      char *buffer = (char *)mrb_malloc(mrb, str_len + 1);
      copy_string_value(ref_id, buffer, str_len + 1);
      mrb_value str = mrb_str_new_cstr(mrb, buffer);
      mrb_free(mrb, buffer);
      return str;
    }
    default:
      return wrap_ref_as_js_object(mrb, ref_id);
  }
}

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
    int len = RSTRING_LEN(value);
    int ref_id = js_create_object();
    if (ref_id < 0) return -1;
    EM_ASM_({
      const str = UTF8ToString($1, $2);
      globalThis.picorubyRefs[$0] = str;
    }, ref_id, str, len);
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

  mrb_value event = wrap_ref_as_js_object(global_mrb, event_ref_id);

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

  if (mrb_nil_p(task)) {
    mrc_irep_free(cc, irep);
    mrc_ccontext_free(cc);
    fprintf(stderr, "Callback exception (failed to create task)\n");
    return;
  }

  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      fprintf(stderr, "Callback exception (failed to inspect exception)\n");
      global_mrb->exc = NULL;
    } else {
      fprintf(stderr, "Callback exception: %s\n", RSTRING_PTR(exc_str));
    }
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

  mrb_value response = js_ref_to_ruby_value(mrb, result_id);

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
    mrb_value arg_value = js_ref_to_ruby_value(global_mrb, arg_ref_ids[i]);
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
  struct RProc *proc = mrb_proc_new(global_mrb, (const mrb_irep *)irep);
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
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      fprintf(stderr, "Generic callback exception (failed to inspect exception)\n");
      global_mrb->exc = NULL;
    } else {
      fprintf(stderr, "Generic callback exception: %s\n", RSTRING_PTR(exc_str));
    }
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
  return js_ref_to_ruby_value(mrb, ref_id);
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
    success = set_property(ref_id, property_name, RSTRING_PTR(value), RSTRING_LEN(value));
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

  uintptr_t task_ptr = (uintptr_t)mrb_ptr(current_task);

  mrb_suspend_task(mrb, current_task);
  setup_binary_handler(js_obj->ref_id, (uintptr_t)mrb, task_ptr, (uintptr_t)callback_id);

  return mrb_nil_value();
}

/*
 * JS::Object#_await_and_suspend
 * Generic Promise await. self must be a JS Promise object.
 * Suspends the current Ruby task until the Promise resolves,
 * then stores the result in $promise_responses[callback_id].
 */
static mrb_value
mrb_object__await_and_suspend(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  mrb_int callback_id;
  mrb_get_args(mrb, "i", &callback_id);

  mrb_value current_task = mrb_funcall_id(mrb, mrb_obj_value(mrb_class_get_id(mrb, MRB_SYM(Task))), MRB_SYM(current), 0);
  uintptr_t task_ptr = (uintptr_t)mrb_ptr(current_task);

  mrb_suspend_task(mrb, current_task);
  setup_promise_handler(obj->ref_id, (uintptr_t)callback_id, (uintptr_t)mrb, task_ptr);

  return mrb_nil_value();
}

/*
 * JS::Object#==
 * Since Phase 2, primitive JS values are auto-converted to Ruby native
 * values, so self here is always a composite JS::Object (object / array /
 * function / symbol / bigint). Equality therefore reduces to ref_id match.
 */
static mrb_value
mrb_object_eq(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(self);
  mrb_value other;
  mrb_get_args(mrb, "o", &other);

  if (mrb_obj_is_kind_of(mrb, other, class_JS_Object)) {
    picorb_js_obj *other_obj = (picorb_js_obj *)DATA_PTR(other);
    return mrb_bool_value(js_obj->ref_id == other_obj->ref_id);
  }
  return mrb_false_value();
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
 * Build a JSON-encoded argument array and call a JS method.
 * Handles any combination of String, Integer, Float, nil, bool, JS::Object.
 * Returns the ref_id of the result, or -1 on error.
 */
/*
 * Build a JSON-encoded tagged-value argument array for the EM_JS call helpers.
 * `context` is a human-readable label used in error messages (e.g. the method
 * name or "constructor"). Passing a non-negative `extra_ref_id` appends one
 * more argument of type "ref" after argv[].
 * Raises on unsupported argument types; does not return in that case.
 */
static mrb_value
build_args_json(mrb_state *mrb, mrb_value *argv, mrb_int argc, int extra_ref_id, const char *context)
{
  mrb_value json_array = mrb_str_new_lit(mrb, "[");
  mrb_int total = argc + (extra_ref_id >= 0 ? 1 : 0);

  for (mrb_int i = 0; i < total; i++) {
    if (i > 0) mrb_str_cat_lit(mrb, json_array, ",");
    mrb_str_cat_lit(mrb, json_array, "{\"type\":");

    if (i == argc && extra_ref_id >= 0) {
      mrb_str_cat_lit(mrb, json_array, "\"ref\",\"value\":");
      char ref_buf[32];
      snprintf(ref_buf, sizeof(ref_buf), "%d", extra_ref_id);
      mrb_str_cat_cstr(mrb, json_array, ref_buf);
    } else if (mrb_string_p(argv[i])) {
      mrb_str_cat_lit(mrb, json_array, "\"string\",\"value\":\"");
      const char *str = mrb_string_value_cstr(mrb, &argv[i]);
      for (const char *p = str; *p; p++) {
        if (*p == '"' || *p == '\\') {
          char escaped[3] = {'\\', *p, '\0'};
          mrb_str_cat(mrb, json_array, escaped, 2);
        } else {
          mrb_str_cat(mrb, json_array, p, 1);
        }
      }
      mrb_str_cat_lit(mrb, json_array, "\"");
    } else if (mrb_integer_p(argv[i])) {
      mrb_str_cat_lit(mrb, json_array, "\"integer\",\"value\":");
      char num_buf[32];
      snprintf(num_buf, sizeof(num_buf), "%d", (int)mrb_integer(argv[i]));
      mrb_str_cat_cstr(mrb, json_array, num_buf);
    } else if (mrb_float_p(argv[i])) {
      mrb_str_cat_lit(mrb, json_array, "\"float\",\"value\":");
      char num_buf[64];
      snprintf(num_buf, sizeof(num_buf), "%f", mrb_float(argv[i]));
      mrb_str_cat_cstr(mrb, json_array, num_buf);
    } else if (mrb_nil_p(argv[i])) {
      mrb_str_cat_lit(mrb, json_array, "\"nil\",\"value\":null");
    } else if (mrb_true_p(argv[i]) || mrb_false_p(argv[i])) {
      mrb_str_cat_lit(mrb, json_array, "\"boolean\",\"value\":");
      mrb_str_cat_cstr(mrb, json_array, mrb_true_p(argv[i]) ? "true" : "false");
    } else if (mrb_obj_is_kind_of(mrb, argv[i], class_JS_Object)) {
      picorb_js_obj *arg_obj = (picorb_js_obj *)DATA_PTR(argv[i]);
      mrb_str_cat_lit(mrb, json_array, "\"ref\",\"value\":");
      char ref_buf[32];
      snprintf(ref_buf, sizeof(ref_buf), "%d", arg_obj->ref_id);
      mrb_str_cat_cstr(mrb, json_array, ref_buf);
    } else {
      mrb_raisef(mrb, E_TYPE_ERROR, "Unsupported argument type for %s at position: %d", context, (int)i);
    }

    mrb_str_cat_lit(mrb, json_array, "}");
  }

  mrb_str_cat_lit(mrb, json_array, "]");
  return json_array;
}

static int
call_method_with_ruby_args(mrb_state *mrb, int ref_id, const char *method_name, mrb_value *argv, mrb_int argc, int extra_ref_id)
{
  mrb_value json = build_args_json(mrb, argv, argc, extra_ref_id, method_name);
  return call_method_with_args(ref_id, method_name, RSTRING_PTR(json), RSTRING_LEN(json));
}

static int
call_constructor_with_ruby_args(mrb_state *mrb, int ref_id, mrb_value *argv, mrb_int argc, int extra_ref_id)
{
  mrb_value json = build_args_json(mrb, argv, argc, extra_ref_id, "constructor");
  return call_constructor_with_args(ref_id, RSTRING_PTR(json), RSTRING_LEN(json));
}

static int
apply_function_with_ruby_args(mrb_state *mrb, int func_ref_id, mrb_value *argv, mrb_int argc)
{
  mrb_value json = build_args_json(mrb, argv, argc, -1, "function call");
  return js_function_apply_args(func_ref_id, RSTRING_PTR(json), RSTRING_LEN(json));
}

static int
register_ruby_block_as_js_callback(mrb_state *mrb, mrb_value blk)
{
  mrb_value callback_id_val = mrb_funcall_id(mrb, blk, mrb_intern_lit(mrb, "object_id"), 0);
  if (!mrb_integer_p(callback_id_val)) {
    mrb_raise(mrb, E_TYPE_ERROR, "block object_id must be Integer");
  }
  mrb_int callback_id = mrb_integer(callback_id_val);

  mrb_value callbacks = mrb_const_get(mrb, mrb_obj_value(class_JS_Object), mrb_intern_lit(mrb, "CALLBACKS"));
  if (!mrb_hash_p(callbacks)) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object::CALLBACKS is not Hash");
  }
  mrb_hash_set(mrb, callbacks, mrb_fixnum_value(callback_id), blk);

  int callback_ref_id = js_create_callback_function((uintptr_t)callback_id);
  if (callback_ref_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "failed to create JS callback function");
  }
  return callback_ref_id;
}

/*
 * JS::Object#method_missing
 */
static mrb_value
mrb_object_method_missing(mrb_state *mrb, mrb_value self)
{
  mrb_sym method_sym;
  mrb_value *argv;
  mrb_value blk = mrb_nil_value();
  mrb_int argc;
  mrb_get_args(mrb, "n*&", &method_sym, &argv, &argc, &blk);
  const char *method_name = mrb_sym_name(mrb, method_sym);
  picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(self);

  int self_js_type = get_js_type(js_obj->ref_id);
  if (self_js_type == JS_TYPE_UNDEFINED || self_js_type == JS_TYPE_NULL) {
    return mrb_nil_value();
  }
  bool has_block = !mrb_nil_p(blk);
  int callback_ref_id = -1;
  if (has_block) {
    callback_ref_id = register_ruby_block_as_js_callback(mrb, blk);
  }
  bool is_constructor_call = (self_js_type == JS_TYPE_FUNCTION && strcmp(method_name, "new") == 0);

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

  if (is_constructor_call) {
    new_ref_id = call_constructor_with_ruby_args(mrb, js_obj->ref_id, argv, argc, has_block ? callback_ref_id : -1);
    return js_ref_to_ruby_value(mrb, new_ref_id);
  }

  if (has_block) {
    new_ref_id = call_method_with_ruby_args(mrb, js_obj->ref_id, method_name, argv, argc, callback_ref_id);
    return js_ref_to_ruby_value(mrb, new_ref_id);
  }

  if (argc == 0) {
    int js_type = get_js_property_type(js_obj->ref_id, method_name);
    if (js_type == JS_TYPE_FUNCTION) {
      new_ref_id = call_method_no_arg(js_obj->ref_id, method_name);
      return js_ref_to_ruby_value(mrb, new_ref_id);
    }
    // Try to get property
    mrb_value property_obj = get_js_property(mrb, js_obj->ref_id, method_name);
    if (!mrb_nil_p(property_obj)) {
      return property_obj;
    }
    // If property doesn't exist, try calling as method
    new_ref_id = call_method_no_arg(js_obj->ref_id, method_name);
    if (new_ref_id < 0) {
      return mrb_nil_value();
    }
  } else if (argc == 1) { // One argument
    new_ref_id = -1;
    if (mrb_string_p(argv[0])) {
      new_ref_id = call_method(js_obj->ref_id, method_name, RSTRING_PTR(argv[0]), RSTRING_LEN(argv[0]));
    } else if (mrb_integer_p(argv[0])) {
      new_ref_id = call_method_int(js_obj->ref_id, method_name, mrb_integer(argv[0]));
    } else if (mrb_obj_is_kind_of(mrb, argv[0], class_JS_Object)) {
      picorb_js_obj *arg_obj = (picorb_js_obj *)DATA_PTR(argv[0]);
      new_ref_id = call_method_with_ref(js_obj->ref_id, method_name, arg_obj->ref_id);
    } else {
      // Fall back to the generic JSON dispatch so boolean / nil / Float
      // (and any other supported Ruby type) are forwarded correctly.
      new_ref_id = call_method_with_ruby_args(mrb, js_obj->ref_id, method_name, argv, argc, -1);
    }
    return js_ref_to_ruby_value(mrb, new_ref_id);
  } else if (argc == 2) {
    new_ref_id = -1;
    if (mrb_obj_is_kind_of(mrb, argv[0], class_JS_Object) && mrb_obj_is_kind_of(mrb, argv[1], class_JS_Object)) {
      picorb_js_obj *arg_obj_1 = (picorb_js_obj *)DATA_PTR(argv[0]);
      picorb_js_obj *arg_obj_2 = (picorb_js_obj *)DATA_PTR(argv[1]);
      new_ref_id = call_method_with_ref_ref(js_obj->ref_id, method_name, arg_obj_1->ref_id, arg_obj_2->ref_id);
    } else if (mrb_string_p(argv[0]) && mrb_string_p(argv[1])) {
      new_ref_id = call_method_str(js_obj->ref_id, method_name, RSTRING_PTR(argv[0]), RSTRING_LEN(argv[0]), RSTRING_PTR(argv[1]), RSTRING_LEN(argv[1]));
    } else {
      new_ref_id = call_method_with_ruby_args(mrb, js_obj->ref_id, method_name, argv, argc, -1);
    }
    return js_ref_to_ruby_value(mrb, new_ref_id);
  } else if (argc == 3) {
    new_ref_id = -1;
    if (mrb_obj_is_kind_of(mrb, argv[0], class_JS_Object) && mrb_string_p(argv[1]) && mrb_string_p(argv[2])) {
      picorb_js_obj *arg_obj_1 = (picorb_js_obj *)DATA_PTR(argv[0]);
      new_ref_id = call_method_with_ref_str_str(js_obj->ref_id, method_name, arg_obj_1->ref_id, RSTRING_PTR(argv[1]), RSTRING_LEN(argv[1]), RSTRING_PTR(argv[2]), RSTRING_LEN(argv[2]));
    } else {
      new_ref_id = call_method_with_ruby_args(mrb, js_obj->ref_id, method_name, argv, argc, -1);
    }
    return js_ref_to_ruby_value(mrb, new_ref_id);
  } else {
    // argc >= 4: generic JSON-based dispatch for any argument types
    new_ref_id = call_method_with_ruby_args(mrb, js_obj->ref_id, method_name, argv, argc, -1);
    return js_ref_to_ruby_value(mrb, new_ref_id);
  }

  return mrb_nil_value();
}


/*
 * JS::Object#to_a
 */
static mrb_value
mrb_object_to_a(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(self);
  int ref_id = js_obj->ref_id;

  // First, check if the object is actually array-like
  int length_ref_id = get_property(ref_id, "length");
  if (length_ref_id < 0) return mrb_ary_new(mrb); // Not array-like
  if (get_js_type(length_ref_id) != 3) return mrb_ary_new(mrb); // length is not a number

  int length = (int)get_number_value(length_ref_id);

  mrb_value array = mrb_ary_new_capa(mrb, length);
  for (int i = 0; i < length; i++) {
    int element_ref_id = get_element(ref_id, i);
    mrb_value element = js_ref_to_ruby_value(mrb, element_ref_id);
    mrb_ary_push(mrb, array, element);
  }
  return array;
}

/*
 * JS::Function#call(*args)
 * Invokes the wrapped JS function value directly with no `this` binding.
 */
static mrb_value
mrb_function_call(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(self);
  mrb_value *argv;
  mrb_int argc;
  mrb_get_args(mrb, "*", &argv, &argc);
  int result_ref_id = apply_function_with_ruby_args(mrb, js_obj->ref_id, argv, argc);
  return js_ref_to_ruby_value(mrb, result_ref_id);
}

/*
 * JS::Object#to_s
 * Since Phase 2, primitive JS values are auto-converted to Ruby native
 * strings/numbers at the C boundary. This fallback runs only for composite
 * JS::Object (object / array / function / symbol / bigint) and returns a
 * short identifier; use #inspect for the human-readable preview.
 */
static mrb_value
mrb_object_to_s(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *js_obj = (picorb_js_obj *)DATA_PTR(self);
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "#<JS::Object:0x%x(ref:%d)>",
           (unsigned int)js_obj, js_obj->ref_id);
  return mrb_str_new_cstr(mrb, buffer);
}

/*
 * JS::Object#to_f
 * Fallback for composite JS::Object; numeric JS values are already
 * converted to Ruby Float/Integer and never reach this method.
 */
static mrb_value
mrb_object_to_f(mrb_state *mrb, mrb_value self)
{
  (void)self;
  return mrb_float_value(mrb, 0.0);
}

/*
 * JS::Object#to_i
 * Fallback for composite JS::Object; numeric JS values are already
 * converted to Ruby Integer and never reach this method.
 */
static mrb_value
mrb_object_to_i(mrb_state *mrb, mrb_value self)
{
  (void)self;
  return mrb_fixnum_value(0);
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
  int promise_id = call_method(obj->ref_id, "fetch", url, strlen(url));

  mrb_value current_task = mrb_funcall_id(mrb, mrb_obj_value(mrb_class_get_id(mrb, MRB_SYM(Task))), MRB_SYM(current), 0);

  uintptr_t task_ptr = (uintptr_t)mrb_ptr(current_task);

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

  uintptr_t task_ptr = (uintptr_t)mrb_ptr(current_task);

  mrb_suspend_task(mrb, current_task);
  setup_promise_handler(promise_id, (uintptr_t)callback_id, (uintptr_t)mrb, task_ptr);

  return mrb_nil_value();
}

/*
 * JS::Object#_set_timeout
 * setTimeout implementation using callback pattern
 */
static mrb_value
mrb_object__set_timeout(mrb_state *mrb, mrb_value self)
{
  mrb_int callback_id;
  mrb_int delay_ms;
  mrb_get_args(mrb, "ii", &callback_id, &delay_ms);
  int timer_id = js_set_timeout((uintptr_t)callback_id, (int)delay_ms);
  return mrb_fixnum_value(timer_id);
}

/*
 * JS::Object#_clear_timeout
 * clearTimeout implementation
 */
static mrb_value
mrb_object__clear_timeout(mrb_state *mrb, mrb_value self)
{
  mrb_int callback_id;
  mrb_get_args(mrb, "i", &callback_id);
  bool success = js_clear_timeout((uintptr_t)callback_id);
  return mrb_bool_value(success);
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
  return js_ref_to_ruby_value(mrb, ref_id);
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
  return js_ref_to_ruby_value(mrb, ref_id);
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
  return js_ref_to_ruby_value(mrb, ref_id);
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
  return js_ref_to_ruby_value(mrb, ref_id);
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
  return wrap_ref_as_js_object(mrb, 0);
}

static mrb_value
mrb_js_refcount(mrb_state *mrb, mrb_value self)
{
  // mruby doesn't expose refcount in the same way as mruby/c
  // Return a placeholder value
  return mrb_fixnum_value(0);
}

EM_JS(int, js_eval, (const char* script), {
  try {
    var result = (0, eval)(UTF8ToString(script));
    if (result === undefined || result === null) return -1;
    var refId = globalThis.picorubyRefs.length;
    globalThis.picorubyRefs.push(result);
    return refId;
  } catch(e) {
    console.error('JS.eval error:', e);
    return -1;
  }
});

/*
 * JS.eval(script) - Evaluate JavaScript code synchronously
 */
static mrb_value
mrb_js_eval(mrb_state *mrb, mrb_value klass)
{
  const char *script;
  mrb_get_args(mrb, "z", &script);
  int ref_id = js_eval(script);
  if (ref_id < 0) {
    return mrb_nil_value();
  }
  return js_ref_to_ruby_value(mrb, ref_id);
}

/*
 * Build a short, human-readable preview of the JS value at ref_id
 * and write it into buf (UTF-8, NUL-terminated).
 * Used by JS::Object#inspect.
 */
EM_JS(void, js_inspect_to_buffer, (int ref_id, char* buf, int buf_size), {
  function clip(s, max) {
    if (s.length > max) return s.slice(0, max - 3) + '...';
    return s;
  }
  function previewValue(val) {
    if (val === null) return 'null';
    if (val === undefined) return 'undefined';
    const t = typeof val;
    if (t === 'string') return JSON.stringify(val);
    if (t === 'number' || t === 'boolean') return String(val);
    if (t === 'bigint') return val.toString() + 'n';
    if (t === 'symbol') return val.toString();
    if (t === 'function') return 'function';
    return '...';
  }
  let result;
  try {
    const v = globalThis.picorubyRefs[ref_id];
    if (v === null) {
      result = 'null';
    } else if (v === undefined) {
      result = 'undefined';
    } else if (typeof v === 'string') {
      result = 'String ' + clip(JSON.stringify(v), 120);
    } else if (typeof v === 'number') {
      result = 'Number ' + String(v);
    } else if (typeof v === 'boolean') {
      result = 'Boolean ' + (v ? 'true' : 'false');
    } else if (typeof v === 'bigint') {
      result = 'BigInt ' + v.toString() + 'n';
    } else if (typeof v === 'symbol') {
      result = 'Symbol ' + v.toString();
    } else if (typeof v === 'function') {
      const name = v.name && v.name.length > 0 ? v.name : '(anonymous)';
      result = 'Function ' + name;
    } else if (Array.isArray(v)) {
      const len = v.length;
      const sample = v.slice(0, 5).map(previewValue).join(',');
      const preview = len > 5 ? '[' + sample + ',...]' : '[' + sample + ']';
      result = 'Array length=' + len + ' ' + clip(preview, 120);
    } else {
      let ctor = 'Object';
      try {
        if (v.constructor && v.constructor.name) ctor = v.constructor.name;
      } catch (e) {}
      let extras = '';
      try {
        if (typeof Event !== 'undefined' && v instanceof Event) {
          if (v.type) extras += ' type=' + JSON.stringify(String(v.type));
        } else if (typeof Response !== 'undefined' && v instanceof Response) {
          if (v.status !== undefined) extras += ' status=' + v.status;
          if (v.url) extras += ' url=' + JSON.stringify(String(v.url));
        } else if (typeof Element !== 'undefined' && v instanceof Element) {
          if (v.id) extras += ' id=' + JSON.stringify(String(v.id));
          const cls = v.getAttribute && v.getAttribute('class');
          if (cls) extras += ' class=' + JSON.stringify(String(cls));
        } else {
          const keys = Object.keys(v).slice(0, 3);
          if (keys.length > 0) {
            const items = keys.map(function(k) {
              return k + '=' + previewValue(v[k]);
            });
            extras = ' ' + items.join(' ');
            if (Object.keys(v).length > 3) extras += ' ...';
          }
        }
      } catch (e) {}
      result = ctor + extras;
    }
  } catch (e) {
    result = '<inspect error: ' + (e && e.message ? e.message : 'unknown') + '>';
  }
  if (buf_size > 0) {
    if (result.length > buf_size - 1) {
      result = result.slice(0, buf_size - 4) + '...';
    }
    stringToUTF8(result, buf, buf_size);
  }
});

/*
 * JS::Object#inspect
 * Returns a readable representation such as:
 *   #<JS::Object ref:87 HTMLDivElement id="foo" class="bar">
 *   #<JS::Object ref:42 Array length=3 [1,2,"x"]>
 *   #<JS::Object ref:8 Number 3.14>
 */
static mrb_value
mrb_object_inspect(mrb_state *mrb, mrb_value self)
{
  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(self);
  char body[384];
  body[0] = '\0';
  js_inspect_to_buffer(obj->ref_id, body, sizeof(body));

  char head[48];
  snprintf(head, sizeof(head), "#<JS::Object ref:%d ", obj->ref_id);

  mrb_value result = mrb_str_new_cstr(mrb, head);
  mrb_str_cat_cstr(mrb, result, body);
  mrb_str_cat_lit(mrb, result, ">");
  return result;
}


void
mrb_js_init(mrb_state *mrb)
{
  init_js_refs();
  init_js_type_offsets(TYPE_OFFSET, VALUE_OFFSET, IS_INTEGER_OFFSET, STRING_VALUE_OFFSET);

  struct RClass *module_JS = mrb_define_module(mrb, "JS");
  mrb_define_class_method_id(mrb, module_JS, MRB_SYM(global), mrb_js_global, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, module_JS, MRB_SYM(eval), mrb_js_eval, MRB_ARGS_REQ(1));

  class_JS_Object = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(Object), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_JS_Object, MRB_TT_DATA);

  mrb_define_method_id(mrb, class_JS_Object, MRB_OPSYM(aref), mrb_object_get_property, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_OPSYM(aset), mrb_object_set_property, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_OPSYM(eq), mrb_object_eq, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(method_missing), mrb_object_method_missing, MRB_ARGS_ANY());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(to_a), mrb_object_to_a, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(to_s), mrb_object_to_s, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(inspect), mrb_object_inspect, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(to_f), mrb_object_to_f, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(to_i), mrb_object_to_i, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_add_event_listener), mrb_object__add_event_listener, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_JS_Object, MRB_SYM(_register_callback), mrb_object_s__register_callback, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_fetch_and_suspend), mrb_object__fetch_and_suspend, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_fetch_with_options_and_suspend), mrb_object__fetch_with_options_and_suspend, MRB_ARGS_REQ(3));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_await_and_suspend), mrb_object__await_and_suspend, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_set_timeout), mrb_object__set_timeout, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_clear_timeout), mrb_object__clear_timeout, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(typeof), mrb_object_type, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(refcount), mrb_js_refcount, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(_removeEventListener), mrb_object__remove_event_listener, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(create_object), mrb_object_create_object, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Object, MRB_SYM(create_array), mrb_object_create_array, MRB_ARGS_NONE());

  // Composite-type subclasses. wrap_ref_as_js_object dispatches to these
  // based on the JS runtime type of the wrapped value.
  class_JS_Array = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(Array), class_JS_Object);
  MRB_SET_INSTANCE_TT(class_JS_Array, MRB_TT_DATA);

  class_JS_Function = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(Function), class_JS_Object);
  MRB_SET_INSTANCE_TT(class_JS_Function, MRB_TT_DATA);
  mrb_define_method_id(mrb, class_JS_Function, MRB_SYM(call), mrb_function_call, MRB_ARGS_ANY());

  class_JS_Promise = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(Promise), class_JS_Object);
  MRB_SET_INSTANCE_TT(class_JS_Promise, MRB_TT_DATA);

  // DOM-domain subclasses for Event / Response / Element. Methods that only
  // make sense on these types live here so type checkers can reject misuse
  // (e.g. calling preventDefault on a plain JS::Object).
  class_JS_Event = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(Event), class_JS_Object);
  MRB_SET_INSTANCE_TT(class_JS_Event, MRB_TT_DATA);
  mrb_define_method_id(mrb, class_JS_Event, MRB_SYM(preventDefault), mrb_object_prevent_default, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_JS_Event, MRB_SYM(stopPropagation), mrb_object_stop_propagation, MRB_ARGS_NONE());

  class_JS_Response = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(Response), class_JS_Object);
  MRB_SET_INSTANCE_TT(class_JS_Response, MRB_TT_DATA);
  mrb_define_method_id(mrb, class_JS_Response, MRB_SYM(_to_binary_and_suspend), mrb_object__to_binary_and_suspend, MRB_ARGS_REQ(1));

  class_JS_Element = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(Element), class_JS_Object);
  MRB_SET_INSTANCE_TT(class_JS_Element, MRB_TT_DATA);
  mrb_define_method_id(mrb, class_JS_Element, MRB_SYM(createElement), mrb_object_create_element, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Element, MRB_SYM(createTextNode), mrb_object_create_text_node, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Element, MRB_SYM(appendChild), mrb_object_append_child, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Element, MRB_SYM(removeChild), mrb_object_remove_child, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_JS_Element, MRB_SYM(replaceChild), mrb_object_replace_child, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Element, MRB_SYM(insertBefore), mrb_object_insert_before, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Element, MRB_SYM(setAttribute), mrb_object_set_attribute, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_JS_Element, MRB_SYM(removeAttribute), mrb_object_remove_attribute, MRB_ARGS_REQ(1));
}
