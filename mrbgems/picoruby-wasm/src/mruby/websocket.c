#include <emscripten.h>

#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mruby/variable.h"

#include "mruby_compiler.h"
#include "task.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct picorb_websocket {
  int ref_id;
} picorb_websocket;

static struct RClass *class_WebSocket;

static void
picorb_websocket_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

static const struct mrb_data_type picorb_websocket_type = {
  "picorb_websocket", picorb_websocket_free
};

EM_JS(int, ws_new, (const char* url), {
  try {
    const ws = new WebSocket(UTF8ToString(url));
    const refId = globalThis.picorubyRefs.push(ws) - 1;
    return refId;
  } catch(e) {
    console.error('WebSocket creation failed:', e);
    return -1;
  }
});

EM_JS(void, ws_send, (int ref_id, const char* data), {
  try {
    const ws = globalThis.picorubyRefs[ref_id];
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(UTF8ToString(data));
    }
  } catch(e) {
    console.error('WebSocket send failed:', e);
  }
});

EM_JS(void, ws_send_binary, (int ref_id, const uint8_t* data, int length), {
  try {
    const ws = globalThis.picorubyRefs[ref_id];
    if (ws && ws.readyState === WebSocket.OPEN) {
      const buffer = new Uint8Array(HEAPU8.buffer, data, length);
      const copy = new Uint8Array(buffer);
      ws.send(copy.buffer);
    }
  } catch(e) {
    console.error('WebSocket send_binary failed:', e);
  }
});

EM_JS(void, ws_close, (int ref_id), {
  try {
    const ws = globalThis.picorubyRefs[ref_id];
    if (ws) {
      ws.close();
    }
  } catch(e) {
    console.error('WebSocket close failed:', e);
  }
});

EM_JS(int, ws_ready_state, (int ref_id), {
  try {
    const ws = globalThis.picorubyRefs[ref_id];
    if (ws) {
      return ws.readyState;
    }
    return -1;
  } catch(e) {
    return -1;
  }
});

EM_JS(void, ws_set_onopen, (int ref_id, uintptr_t callback_id), {
  const ws = globalThis.picorubyRefs[ref_id];
  ws.onopen = (event) => {
    const eventRefId = globalThis.picorubyRefs.push(event) - 1;
    ccall(
      'call_ruby_callback',
      'void',
      ['number', 'number'],
      [callback_id, eventRefId]
    );
  };
});

EM_JS(void, ws_set_onmessage, (int ref_id, uintptr_t callback_id), {
  const ws = globalThis.picorubyRefs[ref_id];
  ws.onmessage = (event) => {
    const eventRefId = globalThis.picorubyRefs.push(event) - 1;
    ccall(
      'call_ruby_callback',
      'void',
      ['number', 'number'],
      [callback_id, eventRefId]
    );
  };
});

EM_JS(void, ws_set_onerror, (int ref_id, uintptr_t callback_id), {
  const ws = globalThis.picorubyRefs[ref_id];
  ws.onerror = (event) => {
    const eventRefId = globalThis.picorubyRefs.push(event) - 1;
    ccall(
      'call_ruby_callback',
      'void',
      ['number', 'number'],
      [callback_id, eventRefId]
    );
  };
});

EM_JS(void, ws_set_onclose, (int ref_id, uintptr_t callback_id), {
  const ws = globalThis.picorubyRefs[ref_id];
  ws.onclose = (event) => {
    const eventRefId = globalThis.picorubyRefs.push(event) - 1;
    ccall(
      'call_ruby_callback',
      'void',
      ['number', 'number'],
      [callback_id, eventRefId]
    );
  };
});

static mrb_value
mrb_websocket_initialize(mrb_state *mrb, mrb_value self)
{
  const char *url;
  mrb_get_args(mrb, "z", &url);

  int ref_id = ws_new(url);
  if (ref_id < 0) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "Failed to create WebSocket");
  }

  picorb_websocket *data = (picorb_websocket *)mrb_malloc(mrb, sizeof(picorb_websocket));
  data->ref_id = ref_id;
  DATA_PTR(self) = data;
  DATA_TYPE(self) = &picorb_websocket_type;

  return self;
}

static mrb_value
mrb_websocket_send(mrb_state *mrb, mrb_value self)
{
  const char *data;
  mrb_get_args(mrb, "z", &data);

  picorb_websocket *ws = (picorb_websocket *)DATA_PTR(self);
  if (!ws) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "WebSocket not initialized");
  }

  ws_send(ws->ref_id, data);
  return mrb_nil_value();
}

static mrb_value
mrb_websocket_send_binary(mrb_state *mrb, mrb_value self)
{
  mrb_value data_str;
  mrb_get_args(mrb, "S", &data_str);

  picorb_websocket *ws = (picorb_websocket *)DATA_PTR(self);
  if (!ws) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "WebSocket not initialized");
  }

  const uint8_t *data = (const uint8_t *)RSTRING_PTR(data_str);
  int length = RSTRING_LEN(data_str);

  ws_send_binary(ws->ref_id, data, length);
  return mrb_nil_value();
}

static mrb_value
mrb_websocket_close(mrb_state *mrb, mrb_value self)
{
  picorb_websocket *ws = (picorb_websocket *)DATA_PTR(self);
  if (!ws) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "WebSocket not initialized");
  }

  ws_close(ws->ref_id);
  return mrb_nil_value();
}

static mrb_value
mrb_websocket_ready_state(mrb_state *mrb, mrb_value self)
{
  picorb_websocket *ws = (picorb_websocket *)DATA_PTR(self);
  if (!ws) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "WebSocket not initialized");
  }

  int state = ws_ready_state(ws->ref_id);
  return mrb_fixnum_value(state);
}

static mrb_value
mrb_websocket_set_onopen(mrb_state *mrb, mrb_value self)
{
  mrb_int callback_id;
  mrb_get_args(mrb, "i", &callback_id);

  picorb_websocket *ws = (picorb_websocket *)DATA_PTR(self);
  if (!ws) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "WebSocket not initialized");
  }

  ws_set_onopen(ws->ref_id, (uintptr_t)callback_id);
  return mrb_nil_value();
}

static mrb_value
mrb_websocket_set_onmessage(mrb_state *mrb, mrb_value self)
{
  mrb_int callback_id;
  mrb_get_args(mrb, "i", &callback_id);

  picorb_websocket *ws = (picorb_websocket *)DATA_PTR(self);
  if (!ws) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "WebSocket not initialized");
  }

  ws_set_onmessage(ws->ref_id, (uintptr_t)callback_id);
  return mrb_nil_value();
}

static mrb_value
mrb_websocket_set_onerror(mrb_state *mrb, mrb_value self)
{
  mrb_int callback_id;
  mrb_get_args(mrb, "i", &callback_id);

  picorb_websocket *ws = (picorb_websocket *)DATA_PTR(self);
  if (!ws) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "WebSocket not initialized");
  }

  ws_set_onerror(ws->ref_id, (uintptr_t)callback_id);
  return mrb_nil_value();
}

static mrb_value
mrb_websocket_set_onclose(mrb_state *mrb, mrb_value self)
{
  mrb_int callback_id;
  mrb_get_args(mrb, "i", &callback_id);

  picorb_websocket *ws = (picorb_websocket *)DATA_PTR(self);
  if (!ws) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "WebSocket not initialized");
  }

  ws_set_onclose(ws->ref_id, (uintptr_t)callback_id);
  return mrb_nil_value();
}

void
mrb_websocket_init(mrb_state *mrb)
{
  class_WebSocket = mrb_define_class_id(mrb, MRB_SYM(WebSocket), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_WebSocket, MRB_TT_DATA);

  mrb_define_method_id(mrb, class_WebSocket, MRB_SYM(initialize), mrb_websocket_initialize, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_WebSocket, MRB_SYM(send), mrb_websocket_send, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_WebSocket, MRB_SYM(send_binary), mrb_websocket_send_binary, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_WebSocket, MRB_SYM(close), mrb_websocket_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_WebSocket, MRB_SYM(ready_state), mrb_websocket_ready_state, MRB_ARGS_NONE());
  mrb_define_private_method_id(mrb, class_WebSocket, MRB_SYM(_set_onopen), mrb_websocket_set_onopen, MRB_ARGS_REQ(1));
  mrb_define_private_method_id(mrb, class_WebSocket, MRB_SYM(_set_onmessage), mrb_websocket_set_onmessage, MRB_ARGS_REQ(1));
  mrb_define_private_method_id(mrb, class_WebSocket, MRB_SYM(_set_onerror), mrb_websocket_set_onerror, MRB_ARGS_REQ(1));
  mrb_define_private_method_id(mrb, class_WebSocket, MRB_SYM(_set_onclose), mrb_websocket_set_onclose, MRB_ARGS_REQ(1));

  mrb_define_const_id(mrb, class_WebSocket, MRB_SYM(CONNECTING), mrb_fixnum_value(0));
  mrb_define_const_id(mrb, class_WebSocket, MRB_SYM(OPEN), mrb_fixnum_value(1));
  mrb_define_const_id(mrb, class_WebSocket, MRB_SYM(CLOSING), mrb_fixnum_value(2));
  mrb_define_const_id(mrb, class_WebSocket, MRB_SYM(CLOSED), mrb_fixnum_value(3));
}
