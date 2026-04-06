#include <emscripten.h>

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#include "mruby_compiler.h"
#include "mrc_utils.h"
#include "task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Shared types from js.c */
typedef struct picorb_js_obj {
  int ref_id;
} picorb_js_obj;

extern struct RClass *class_JS_Object;
extern const struct mrb_data_type picorb_js_obj_type;
extern mrb_value wrap_ref_as_js_object(mrb_state *mrb, int ref_id);
extern mrb_state *global_mrb;

/*****************************************************
 * EM_JS helpers for binary data conversion
 *****************************************************/

/* Get byteLength of a DataView */
EM_JS(int, ble_dataview_length, (int ref_id), {
  try {
    const dv = globalThis.picorubyRefs[ref_id];
    if (dv && dv.byteLength !== undefined) {
      return dv.byteLength;
    }
    return 0;
  } catch(e) {
    console.error('ble_dataview_length failed:', e);
    return 0;
  }
});

/* Copy DataView bytes into a C buffer */
EM_JS(int, ble_dataview_read, (int ref_id, uint8_t *out_buf, int max_len), {
  try {
    const dv = globalThis.picorubyRefs[ref_id];
    if (!dv) return 0;
    const len = Math.min(dv.byteLength, max_len);
    for (let i = 0; i < len; i++) {
      HEAPU8[out_buf + i] = dv.getUint8(i);
    }
    return len;
  } catch(e) {
    console.error('ble_dataview_read failed:', e);
    return 0;
  }
});

/* Create a Uint8Array from C buffer, return picorubyRefs index */
EM_JS(int, ble_create_uint8array, (const uint8_t *data, int length), {
  try {
    const buffer = new Uint8Array(HEAPU8.buffer, data, length);
    const copy = new Uint8Array(buffer);
    const refId = globalThis.picorubyRefs.push(copy) - 1;
    return refId;
  } catch(e) {
    console.error('ble_create_uint8array failed:', e);
    return -1;
  }
});

/* Set up characteristicvaluechanged listener with binary data extraction */
EM_JS(void, ble_set_notify_handler, (int char_ref_id, uintptr_t callback_id), {
  try {
    const char_obj = globalThis.picorubyRefs[char_ref_id];
    char_obj.addEventListener('characteristicvaluechanged', (event) => {
      const dataview = event.target.value;
      const len = dataview.byteLength;
      const ptr = _malloc(len);
      for (let i = 0; i < len; i++) {
        HEAPU8[ptr + i] = dataview.getUint8(i);
      }
      ccall(
        'ble_notify_callback',
        'void',
        ['number', 'number', 'number'],
        [callback_id, ptr, len]
      );
      _free(ptr);
    });
  } catch(e) {
    console.error('ble_set_notify_handler failed:', e);
  }
});

/*****************************************************
 * Notification callback (called from JS)
 *****************************************************/

EMSCRIPTEN_KEEPALIVE
void
ble_notify_callback(uintptr_t callback_id, const uint8_t *data, int length)
{
  if (!global_mrb) return;

  /* Push binary data to queue array */
  mrb_sym queue_sym = mrb_intern_lit(global_mrb, "$_ble_notify_queue");
  mrb_value queue = mrb_gv_get(global_mrb, queue_sym);
  if (!mrb_array_p(queue)) {
    queue = mrb_ary_new(global_mrb);
    mrb_gv_set(global_mrb, queue_sym, queue);
  }
  mrb_value data_str = mrb_str_new(global_mrb, (const char *)data, length);
  mrb_ary_push(global_mrb, queue, data_str);

  /* Build and execute callback script */
  static char script[256];
  snprintf(script, sizeof(script),
    "JS::Object::CALLBACKS[%lu]&.call($_ble_notify_queue.shift)",
    (unsigned long)callback_id);

  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  const uint8_t *script_ptr = (const uint8_t *)script;
  size_t size = strlen(script);
  mrc_irep *irep = mrc_load_string_cxt(cc, &script_ptr, size);

  if (!irep) {
    mrc_ccontext_free(cc);
    return;
  }

  mrb_value task = mrc_create_task(cc, irep,
    mrb_nil_value(), mrb_nil_value(),
    mrb_obj_value(global_mrb->object_class));

  if (mrb_nil_p(task)) {
    mrc_irep_free(cc, irep);
    mrc_ccontext_free(cc);
    fprintf(stderr, "BLE notify callback: failed to create task\n");
    return;
  }

  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      fprintf(stderr, "BLE notify callback exception (inspect failed)\n");
      global_mrb->exc = NULL;
    } else {
      fprintf(stderr, "BLE notify callback exception: %s\n",
        RSTRING_PTR(exc_str));
    }
  }
}

/*****************************************************
 * Ruby method implementations
 *****************************************************/

/*
 * JS::BLE._dataview_to_string(js_dataview) -> String
 * Convert a DataView JS object to a binary Ruby String.
 */
static mrb_value
mrb_ble_dataview_to_string(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj;
  mrb_get_args(mrb, "o", &js_obj);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object");
  }

  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(js_obj);
  int len = ble_dataview_length(obj->ref_id);

  if (len <= 0) {
    return mrb_str_new(mrb, "", 0);
  }

  uint8_t *buf = (uint8_t *)mrb_malloc(mrb, len);
  int actual = ble_dataview_read(obj->ref_id, buf, len);
  mrb_value result = mrb_str_new(mrb, (const char *)buf, actual);
  mrb_free(mrb, buf);

  return result;
}

/*
 * JS::BLE._string_to_uint8array(str) -> JS::Object
 * Convert a Ruby String to a Uint8Array JS object.
 */
static mrb_value
mrb_ble_string_to_uint8array(mrb_state *mrb, mrb_value self)
{
  mrb_value data_str;
  mrb_get_args(mrb, "S", &data_str);

  const uint8_t *data = (const uint8_t *)RSTRING_PTR(data_str);
  int length = RSTRING_LEN(data_str);

  int ref_id = ble_create_uint8array(data, length);
  if (ref_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to create Uint8Array");
  }

  return wrap_ref_as_js_object(mrb, ref_id);
}

/*
 * JS::BLE._set_notify_handler(js_characteristic, callback_id) -> nil
 * Set up characteristicvaluechanged event listener that delivers
 * binary String data directly to the Ruby callback.
 */
static mrb_value
mrb_ble_set_notify_handler_method(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj;
  mrb_int callback_id;
  mrb_get_args(mrb, "oi", &js_obj, &callback_id);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object");
  }

  picorb_js_obj *obj = (picorb_js_obj *)DATA_PTR(js_obj);
  ble_set_notify_handler(obj->ref_id, (uintptr_t)callback_id);

  return mrb_nil_value();
}

/*****************************************************
 * Module initialization
 *****************************************************/

void
mrb_ble_init(mrb_state *mrb)
{
  struct RClass *module_JS = mrb_module_get_id(mrb, MRB_SYM(JS));
  struct RClass *module_BLE = mrb_define_module_under_id(mrb, module_JS, MRB_SYM(BLE));

  mrb_define_module_function_id(mrb, module_BLE,
    MRB_SYM(_dataview_to_string),
    mrb_ble_dataview_to_string, MRB_ARGS_REQ(1));

  mrb_define_module_function_id(mrb, module_BLE,
    MRB_SYM(_string_to_uint8array),
    mrb_ble_string_to_uint8array, MRB_ARGS_REQ(1));

  mrb_define_module_function_id(mrb, module_BLE,
    MRB_SYM(_set_notify_handler),
    mrb_ble_set_notify_handler_method, MRB_ARGS_REQ(2));
}
