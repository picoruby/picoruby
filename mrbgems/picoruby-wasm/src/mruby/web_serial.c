#include <emscripten.h>

#include "mruby.h"
#include "mruby/presym.h"
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
 * EM_JS helpers
 *****************************************************/

/* Write binary data to the serial port's writable stream */
EM_JS(void, serial_write, (int ref_id, const uint8_t *data, int len), {
  try {
    const port = globalThis.picorubyRefs[ref_id];
    if (!port || !port.writable) {
      console.error('serial_write: port not writable');
      return;
    }

    if (!globalThis.picorubySerialWriteQueues) {
      globalThis.picorubySerialWriteQueues = Object.create(null);
    }

    const bytes = new Uint8Array(HEAPU8.buffer, data, len).slice();
    const key = String(ref_id);
    const prev = globalThis.picorubySerialWriteQueues[key] || Promise.resolve();

    const next = prev.then(async () => {
      const writer = port.writable.getWriter();
      try {
        await writer.write(bytes);
      } finally {
        writer.releaseLock();
      }
    }).catch((e) => {
      console.error('serial_write queue failed:', e);
    });

    globalThis.picorubySerialWriteQueues[key] = next;
  } catch(e) {
    console.error('serial_write failed:', e);
  }
});

/* Return a Promise that resolves when queued writes for this port are drained */
EM_JS(int, serial_write_drain_promise, (int ref_id), {
  try {
    const key = String(ref_id);
    const q = (globalThis.picorubySerialWriteQueues && globalThis.picorubySerialWriteQueues[key]) || Promise.resolve();
    const p = q.then(() => true).catch((e) => {
      console.error('serial_write_drain failed:', e);
      return false;
    });
    return globalThis.picorubyRefs.push(p) - 1;
  } catch (e) {
    console.error('serial_write_drain_promise failed:', e);
    const p = Promise.resolve(false);
    return globalThis.picorubyRefs.push(p) - 1;
  }
});

/* Start async read loop; calls serial_data_received for each chunk */
EM_JS(void, serial_start_reading, (int ref_id, uintptr_t callback_id), {
  try {
    const port = globalThis.picorubyRefs[ref_id];
    if (!port) {
      console.error('serial_start_reading: port not found');
      return;
    }

    if (!globalThis.picorubySerialReadStates) {
      globalThis.picorubySerialReadStates = Object.create(null);
    }
    const key = String(ref_id);
    const state = globalThis.picorubySerialReadStates[key] || { running: false };
    if (state.running) {
      return;
    }
    state.running = true;
    globalThis.picorubySerialReadStates[key] = state;

    (async () => {
      while (port.readable) {
        const reader = port.readable.getReader();
        try {
          while (true) {
            const { value, done } = await reader.read();
            if (done) break;
            const len = value.length;
            const ptr = _malloc(len);
            HEAPU8.set(value, ptr);
            ccall(
              'serial_data_received',
              'void',
              ['number', 'number', 'number'],
              [callback_id, ptr, len]
            );
            _free(ptr);
          }
        } catch(e) {
          const name = e && e.name ? e.name : "";
          const message = e && e.message ? e.message : String(e);
          const deviceLost = name === 'NetworkError' || message.includes('device has been lost');
          if (!deviceLost) {
            console.error('serial read error:', e);
          }
        } finally {
          reader.releaseLock();
        }
      }
      state.running = false;
    })().catch((e) => {
      state.running = false;
      console.error('serial_start_reading async failed:', e);
    });
  } catch(e) {
    console.error('serial_start_reading failed:', e);
  }
});

/* Start async read loop and write decoded text to the provided terminal object */
EM_JS(void, serial_read_from_port, (int ref_id, int terminal_ref_id), {
  try {
    const port = globalThis.picorubyRefs[ref_id];
    const terminal = globalThis.picorubyRefs[terminal_ref_id];
    if (!port) {
      console.error('serial_read_from_port: port not found');
      return;
    }

    if (!globalThis.picorubySerialReadStates) {
      globalThis.picorubySerialReadStates = Object.create(null);
    }
    const key = String(ref_id);
    const state = globalThis.picorubySerialReadStates[key] || { running: false, reader: null };
    if (state.running) {
      return;
    }
    state.running = true;
    globalThis.picorubySerialReadStates[key] = state;

    (async () => {
      const decoder = new TextDecoder('utf-8');
      while (port.readable) {
        const reader = port.readable.getReader();
        state.reader = reader;
        try {
          while (true) {
            const { value, done } = await reader.read();
            if (done) break;
            if (!value) continue;

            const chars = decoder.decode(value, { stream: true });
            if (globalThis.picorubySerialCapture) {
              globalThis.picorubySerialCapture.append(port, chars);
            }
            if (terminal) terminal.write(chars);
          }
        } catch (e) {
          const name = e && e.name ? e.name : "";
          const message = e && e.message ? e.message : String(e);
          const deviceLost = name === 'NetworkError' || message.includes('device has been lost');
          if (!deviceLost) {
            console.error('serial read error:', e);
          }
        } finally {
          reader.releaseLock();
          if (state.reader === reader) {
            state.reader = null;
          }
        }
      }

      const tail = decoder.decode();
      if (tail) {
        if (globalThis.picorubySerialCapture) {
          globalThis.picorubySerialCapture.append(port, tail);
        }
        if (terminal) terminal.write(tail);
      }
      state.running = false;
      globalThis.dispatchEvent(new CustomEvent('serial-reader-closed'));
    })().catch((e) => {
      state.running = false;
      console.error('serial_read_from_port async failed:', e);
      globalThis.dispatchEvent(new CustomEvent('serial-reader-closed'));
    });
  } catch (e) {
    console.error('serial_read_from_port failed:', e);
  }
});

/* Call port.open(options) and return the Promise ref_id */
EM_JS(int, serial_port_open, (int port_ref_id, int options_ref_id), {
  try {
    const port = globalThis.picorubyRefs[port_ref_id];
    const options = globalThis.picorubyRefs[options_ref_id];
    const promise = port.open(options);
    return globalThis.picorubyRefs.push(promise) - 1;
  } catch(e) {
    console.error('serial_port_open failed:', e);
    return -1;
  }
});

/* Call navigator.serial.requestPort() and return the Promise ref_id */
EM_JS(int, serial_request_port, (), {
  try {
    const serial = navigator && navigator.serial;
    if (!serial || !serial.requestPort) {
      console.error('serial_request_port: Web Serial API is not available');
      return -1;
    }
    const promise = serial.requestPort();
    return globalThis.picorubyRefs.push(promise) - 1;
  } catch(e) {
    console.error('serial_request_port failed:', e);
    return -1;
  }
});

/* Watch navigator.serial connect events and relay as window event */
EM_JS(void, serial_watch_connect_events, (), {
  try {
    const serial = navigator && navigator.serial;
    if (!serial || !serial.addEventListener) {
      return;
    }
    if (globalThis.picorubySerialConnectWatcherInstalled) {
      return;
    }
    globalThis.picorubySerialConnectWatcherInstalled = true;

    serial.addEventListener('connect', (e) => {
      const port = (e && e.target) || (e && e.port) || null;
      if (!port) return;

      globalThis.picorubyLastConnectedSerialPort = port;
      globalThis.dispatchEvent(new CustomEvent('serial-port-connect'));
    });
  } catch (e) {
    console.error('serial_watch_connect_events failed:', e);
  }
});

/* Take and clear the last connected SerialPort (if any) */
EM_JS(int, serial_take_last_connected_port, (), {
  try {
    const port = globalThis.picorubyLastConnectedSerialPort;
    globalThis.picorubyLastConnectedSerialPort = null;
    if (!port) return -1;
    return globalThis.picorubyRefs.push(port) - 1;
  } catch (e) {
    console.error('serial_take_last_connected_port failed:', e);
    return -1;
  }
});

/* Call port.close() — fire and forget */
EM_JS(void, serial_port_close, (int ref_id), {
  try {
    const port = globalThis.picorubyRefs[ref_id];
    if (port) port.close();

    const key = String(ref_id);
    if (globalThis.picorubySerialReadStates) {
      delete globalThis.picorubySerialReadStates[key];
    }
    if (globalThis.picorubySerialWriteQueues) {
      delete globalThis.picorubySerialWriteQueues[key];
    }
    if (globalThis.picorubySerialDisconnectHandlers && port) {
      const prev = globalThis.picorubySerialDisconnectHandlers[key];
      if (prev) {
        port.removeEventListener('disconnect', prev);
      }
      delete globalThis.picorubySerialDisconnectHandlers[key];
    }
    if (globalThis.picorubySerialCapture && port) {
      globalThis.picorubySerialCapture.clear(port);
    }
  } catch(e) {
    console.error('serial_port_close failed:', e);
  }
});

/* Set up disconnect event listener */
EM_JS(void, serial_set_on_disconnect, (int ref_id, uintptr_t callback_id), {
  try {
    const port = globalThis.picorubyRefs[ref_id];
    if (!port) {
      console.error('serial_set_on_disconnect: port not found');
      return;
    }
    if (!globalThis.picorubySerialDisconnectHandlers) {
      globalThis.picorubySerialDisconnectHandlers = Object.create(null);
    }

    const key = String(ref_id);
    const prev = globalThis.picorubySerialDisconnectHandlers[key];
    if (prev) {
      port.removeEventListener('disconnect', prev);
    }

    const handler = () => {
      ccall(
        'serial_disconnect_callback',
        'void',
        ['number'],
        [callback_id]
      );
    };
    globalThis.picorubySerialDisconnectHandlers[key] = handler;
    port.addEventListener('disconnect', handler);
  } catch(e) {
    console.error('serial_set_on_disconnect failed:', e);
  }
});

EM_JS(void, serial_capture_start, (int ref_id), {
  try {
    const port = globalThis.picorubyRefs[ref_id];
    if (!port) return;

    if (!globalThis.picorubySerialCapture) {
      const buffers = new WeakMap();
      const active = new WeakSet();
      const MAX_CHARS = 256 * 1024;
      globalThis.picorubySerialCapture = {
        start(p) {
          buffers.set(p, "");
          active.add(p);
        },
        append(p, chunk) {
          if (!p || !active.has(p)) return;
          const prev = buffers.get(p) || "";
          let next = prev + chunk;
          if (next.length > MAX_CHARS) {
            next = next.slice(next.length - MAX_CHARS);
          }
          buffers.set(p, next);
        },
        peek(p) {
          return buffers.get(p) || "";
        },
        stop(p) {
          const out = buffers.get(p) || "";
          active.delete(p);
          return out;
        },
        clear(p) {
          active.delete(p);
          buffers.delete(p);
        },
      };
    }

    globalThis.picorubySerialCapture.start(port);
  } catch (e) {
    console.error('serial_capture_start failed:', e);
  }
});

EM_JS(int, serial_capture_get, (int ref_id, int stop), {
  try {
    const port = globalThis.picorubyRefs[ref_id];
    if (!port || !globalThis.picorubySerialCapture) {
      return globalThis.picorubyRefs.push("") - 1;
    }
    const cap = globalThis.picorubySerialCapture;
    const out = stop ? cap.stop(port) : cap.peek(port);
    return globalThis.picorubyRefs.push(out) - 1;
  } catch (e) {
    console.error('serial_capture_get failed:', e);
    return globalThis.picorubyRefs.push("") - 1;
  }
});

/*****************************************************
 * EMSCRIPTEN_KEEPALIVE callbacks (called from JS)
 *****************************************************/

/*
 * Called from JS for each chunk of received serial data.
 * data pointer is allocated by JS (_malloc) and freed by JS (_free) after this call.
 */
EMSCRIPTEN_KEEPALIVE
void
serial_data_received(uintptr_t callback_id, const uint8_t *data, int length)
{
  if (!global_mrb) return;

  mrb_value data_str = mrb_str_new(global_mrb, (const char *)data, length);
  mrb_gv_set(global_mrb,
    mrb_intern_lit(global_mrb, "$_serial_recv_data"),
    data_str);

  static char script[256];
  snprintf(script, sizeof(script),
    "JS::Object::CALLBACKS[%lu]&.call($_serial_recv_data)",
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
    fprintf(stderr, "serial_data_received: failed to create task\n");
    return;
  }

  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      fprintf(stderr, "serial_data_received exception (inspect failed)\n");
      global_mrb->exc = NULL;
    } else {
      fprintf(stderr, "serial_data_received exception: %s\n", RSTRING_PTR(exc_str));
    }
  }
}

/*
 * Called from JS when the serial port disconnects.
 */
EMSCRIPTEN_KEEPALIVE
void
serial_disconnect_callback(uintptr_t callback_id)
{
  if (!global_mrb) return;

  static char script[256];
  snprintf(script, sizeof(script),
    "JS::Object::CALLBACKS[%lu]&.call()",
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
    fprintf(stderr, "serial_disconnect_callback: failed to create task\n");
    return;
  }
}

/*****************************************************
 * Ruby method implementations (class methods on JS::WebSerial)
 *****************************************************/

/*
 * JS::WebSerial._open_port(js_port, options) -> JS::Object (Promise)
 * Call port.open(options) on the SerialPort, returns the Promise.
 * Exists because Kernel#open is a private method that shadows method_missing.
 */
static mrb_value
mrb_web_serial_open_port(mrb_state *mrb, mrb_value self)
{
  mrb_value port_obj, options_obj;
  mrb_get_args(mrb, "oo", &port_obj, &options_obj);

  if (!mrb_obj_is_kind_of(mrb, port_obj, class_JS_Object) ||
      !mrb_obj_is_kind_of(mrb, options_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(port_obj);
  picorb_js_obj *opts = (picorb_js_obj *)DATA_PTR(options_obj);
  if (!port || !opts) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  int promise_id = serial_port_open(port->ref_id, opts->ref_id);
  if (promise_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "serial_port_open failed");
  }
  return wrap_ref_as_js_object(mrb, promise_id);
}

/*
 * JS::WebSerial._request_port() -> JS::Object (Promise)
 */
static mrb_value
mrb_web_serial_request_port(mrb_state *mrb, mrb_value self)
{
  int promise_id = serial_request_port();
  if (promise_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "serial_request_port failed");
  }
  return wrap_ref_as_js_object(mrb, promise_id);
}

/*
 * JS::WebSerial._watch_connect_events() -> nil
 */
static mrb_value
mrb_web_serial_watch_connect_events(mrb_state *mrb, mrb_value self)
{
  serial_watch_connect_events();
  return mrb_nil_value();
}

/*
 * JS::WebSerial._take_last_connected_port() -> JS::Object? (SerialPort)
 */
static mrb_value
mrb_web_serial_take_last_connected_port(mrb_state *mrb, mrb_value self)
{
  int ref_id = serial_take_last_connected_port();
  if (ref_id < 0) {
    return mrb_nil_value();
  }
  return wrap_ref_as_js_object(mrb, ref_id);
}

/*
 * JS::WebSerial._close_port(js_port) -> nil
 * Call port.close() on the SerialPort.
 */
static mrb_value
mrb_web_serial_close_port(mrb_state *mrb, mrb_value self)
{
  mrb_value port_obj;
  mrb_get_args(mrb, "o", &port_obj);

  if (!mrb_obj_is_kind_of(mrb, port_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(port_obj);
  if (!port) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  serial_port_close(port->ref_id);
  return mrb_nil_value();
}

/*
 * JS::WebSerial._write(js_port, str) -> nil
 * Write binary String data to the serial port.
 */
static mrb_value
mrb_web_serial_write(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj, data_str;
  mrb_get_args(mrb, "oS", &js_obj, &data_str);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(js_obj);
  if (!port) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  const uint8_t *data = (const uint8_t *)RSTRING_PTR(data_str);
  int length = RSTRING_LEN(data_str);

  serial_write(port->ref_id, data, length);
  return mrb_nil_value();
}

/*
 * JS::WebSerial._drain(js_port) -> JS::Object (Promise<boolean>)
 * Resolve when pending serial write queue for this port is drained.
 */
static mrb_value
mrb_web_serial_drain(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj;
  mrb_get_args(mrb, "o", &js_obj);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(js_obj);
  if (!port) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  return wrap_ref_as_js_object(mrb, serial_write_drain_promise(port->ref_id));
}

/*
 * JS::WebSerial._start_reading(js_port, callback_id) -> nil
 * Start async read loop; calls Ruby block for each received chunk.
 */
static mrb_value
mrb_web_serial_start_reading(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj;
  mrb_int callback_id;
  mrb_get_args(mrb, "oi", &js_obj, &callback_id);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(js_obj);
  if (!port) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  serial_start_reading(port->ref_id, (uintptr_t)callback_id);
  return mrb_nil_value();
}

/*
 * JS::WebSerial._read_from_port(js_port, terminal) -> nil
 * Start async read loop that decodes and writes text to terminal directly.
 */
static mrb_value
mrb_web_serial_read_from_port(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj, terminal_obj;
  mrb_get_args(mrb, "oo", &js_obj, &terminal_obj);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object) ||
      !mrb_obj_is_kind_of(mrb, terminal_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(js_obj);
  picorb_js_obj *terminal = (picorb_js_obj *)DATA_PTR(terminal_obj);
  if (!port || !terminal) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  serial_read_from_port(port->ref_id, terminal->ref_id);
  return mrb_nil_value();
}

/*
 * JS::WebSerial._set_on_disconnect(js_port, callback_id) -> nil
 */
static mrb_value
mrb_web_serial_set_on_disconnect(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj;
  mrb_int callback_id;
  mrb_get_args(mrb, "oi", &js_obj, &callback_id);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(js_obj);
  if (!port) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  serial_set_on_disconnect(port->ref_id, (uintptr_t)callback_id);
  return mrb_nil_value();
}

/*
 * JS::WebSerial._capture_start(js_port) -> nil
 */
static mrb_value
mrb_web_serial_capture_start(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj;
  mrb_get_args(mrb, "o", &js_obj);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(js_obj);
  if (!port) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  serial_capture_start(port->ref_id);
  return mrb_nil_value();
}

/*
 * JS::WebSerial._capture_peek(js_port) -> JS::Object (String)
 */
static mrb_value
mrb_web_serial_capture_peek(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj;
  mrb_get_args(mrb, "o", &js_obj);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(js_obj);
  if (!port) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  return wrap_ref_as_js_object(mrb, serial_capture_get(port->ref_id, 0));
}

/*
 * JS::WebSerial._capture_stop(js_port) -> JS::Object (String)
 */
static mrb_value
mrb_web_serial_capture_stop(mrb_state *mrb, mrb_value self)
{
  mrb_value js_obj;
  mrb_get_args(mrb, "o", &js_obj);

  if (!mrb_obj_is_kind_of(mrb, js_obj, class_JS_Object)) {
    mrb_raise(mrb, E_TYPE_ERROR, "expected JS::Object (SerialPort)");
  }

  picorb_js_obj *port = (picorb_js_obj *)DATA_PTR(js_obj);
  if (!port) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "JS::Object has no data");
  }

  return wrap_ref_as_js_object(mrb, serial_capture_get(port->ref_id, 1));
}

/*****************************************************
 * Module initialization
 *****************************************************/

void
mrb_web_serial_init(mrb_state *mrb)
{
  struct RClass *module_JS = mrb_module_get_id(mrb, MRB_SYM(JS));
  struct RClass *class_WebSerial = mrb_define_class_under_id(mrb, module_JS, MRB_SYM(WebSerial), mrb->object_class);

  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_write),
    mrb_web_serial_write, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_drain),
    mrb_web_serial_drain, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_start_reading),
    mrb_web_serial_start_reading, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_read_from_port),
    mrb_web_serial_read_from_port, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_set_on_disconnect),
    mrb_web_serial_set_on_disconnect, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_open_port),
    mrb_web_serial_open_port, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_request_port),
    mrb_web_serial_request_port, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_watch_connect_events),
    mrb_web_serial_watch_connect_events, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_take_last_connected_port),
    mrb_web_serial_take_last_connected_port, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_close_port),
    mrb_web_serial_close_port, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_capture_start),
    mrb_web_serial_capture_start, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_capture_peek),
    mrb_web_serial_capture_peek, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_WebSerial, MRB_SYM(_capture_stop),
    mrb_web_serial_capture_stop, MRB_ARGS_REQ(1));
}
