#include <emscripten.h>

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Shared types from picoruby-wasm/src/mruby/js.c */
typedef struct picorb_js_obj {
  int ref_id;
} picorb_js_obj;

extern struct RClass *class_JS_Object;
extern mrb_value wrap_ref_as_js_object(mrb_state *mrb, int ref_id);

/*****************************************************
 * EM_JS helpers
 *
 * Two entry points only:
 *   1. idb_request_to_promise(req_ref_id)
 *      Wraps an IDBRequest in a Promise so Ruby can use JS::Promise#await.
 *   2. idb_open_with_upgrade(name, version, upgrade_callback_id)
 *      Opens a database. Inside onupgradeneeded the registered Ruby callback
 *      is invoked synchronously via call_ruby_callback_sync_generic so that
 *      schema mutations (createObjectStore / createIndex) run on the JS
 *      stack as required by the spec.
 *****************************************************/

EM_JS(int, idb_request_to_promise, (int req_ref_id), {
  try {
    const req = globalThis.picorubyRefs[req_ref_id];
    if (!req) {
      console.error('idb_request_to_promise: invalid ref_id', req_ref_id);
      return -1;
    }
    const promise = new Promise((resolve, reject) => {
      req.onsuccess = () => { resolve(req.result); };
      req.onerror = () => {
        const msg = (req.error && req.error.message) || 'IDB request failed';
        reject(new Error(msg));
      };
    });
    return globalThis.picorubyRefs.push(promise) - 1;
  } catch (e) {
    console.error('idb_request_to_promise failed:', e);
    return -1;
  }
});

EM_JS(int, idb_open_with_upgrade, (const char *name_ptr, int version, uintptr_t callback_id), {
  try {
    const name = UTF8ToString(name_ptr);
    const idb = globalThis.indexedDB;
    if (!idb) {
      console.error('idb_open_with_upgrade: indexedDB unavailable');
      return -1;
    }
    const req = (version > 0) ? idb.open(name, version) : idb.open(name);

    req.onupgradeneeded = (event) => {
      if (callback_id === 0) return;
      const db = req.result;
      const oldV = event.oldVersion;
      const newV = event.newVersion;

      const dbRef = globalThis.picorubyRefs.push(db) - 1;
      const oldRef = globalThis.picorubyRefs.push(oldV) - 1;
      const newRef = globalThis.picorubyRefs.push(newV) - 1;

      const argRefIds = [dbRef, oldRef, newRef];
      const argRefIdsPtr = _malloc(argRefIds.length * 4);
      for (let i = 0; i < argRefIds.length; i++) {
        HEAP32[(argRefIdsPtr >> 2) + i] = argRefIds[i];
      }
      try {
        ccall(
          'call_ruby_callback_sync_generic',
          'number',
          ['number', 'number', 'number'],
          [callback_id, argRefIdsPtr, argRefIds.length]
        );
      } catch (e) {
        console.error('idb upgrade callback threw:', e);
      } finally {
        _free(argRefIdsPtr);
      }
    };

    const promise = new Promise((resolve, reject) => {
      req.onsuccess = () => { resolve(req.result); };
      req.onerror = () => {
        const msg = (req.error && req.error.message) || 'IDB open failed';
        reject(new Error(msg));
      };
      req.onblocked = () => {
        reject(new Error('IDB open blocked: another connection holds an older version'));
      };
    });
    return globalThis.picorubyRefs.push(promise) - 1;
  } catch (e) {
    console.error('idb_open_with_upgrade failed:', e);
    return -1;
  }
});

/*****************************************************
 * mruby methods
 *****************************************************/

/*
 * IndexedDB::Helper._request_to_promise(js_request) -> JS::Promise
 */
static mrb_value
mrb_idb_helper_request_to_promise(mrb_state *mrb, mrb_value self)
{
  mrb_value req_obj;
  mrb_get_args(mrb, "o", &req_obj);

  if (!mrb_obj_is_kind_of(mrb, req_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (IDBRequest)");
  }
  picorb_js_obj *req = (picorb_js_obj *)DATA_PTR(req_obj);
  if (!req) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }
  int promise_id = idb_request_to_promise(req->ref_id);
  if (promise_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "idb_request_to_promise failed");
  }
  return wrap_ref_as_js_object(mrb, promise_id);
}

/*
 * IndexedDB::Helper._open_with_upgrade(name, version, callback_id) -> JS::Promise
 *
 * callback_id may be 0 to skip the upgrade hook (e.g. when opening at the
 * current version with no schema work to do).
 */
static mrb_value
mrb_idb_helper_open_with_upgrade(mrb_state *mrb, mrb_value self)
{
  const char *name;
  mrb_int version;
  mrb_int callback_id;
  mrb_get_args(mrb, "zii", &name, &version, &callback_id);

  int promise_id = idb_open_with_upgrade(name, (int)version, (uintptr_t)callback_id);
  if (promise_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "idb_open_with_upgrade failed");
  }
  return wrap_ref_as_js_object(mrb, promise_id);
}

/*****************************************************
 * Gem init
 *****************************************************/

void
mrb_picoruby_indexeddb_gem_init(mrb_state *mrb)
{
  struct RClass *module_IndexedDB = mrb_define_module_id(mrb, MRB_SYM(IndexedDB));
  struct RClass *module_Helper = mrb_define_module_under_id(mrb, module_IndexedDB, MRB_SYM(Helper));

  mrb_define_module_function_id(mrb, module_Helper, MRB_SYM(_request_to_promise),
    mrb_idb_helper_request_to_promise, MRB_ARGS_REQ(1));
  mrb_define_module_function_id(mrb, module_Helper, MRB_SYM(_open_with_upgrade),
    mrb_idb_helper_open_with_upgrade, MRB_ARGS_REQ(3));
}

void
mrb_picoruby_indexeddb_gem_final(mrb_state *mrb)
{
}
