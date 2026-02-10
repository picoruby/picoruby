// Debug API for PicoRuby WASM DevTools
// This file provides debugging functions for browser DevTools extension

#include <emscripten.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/proc.h>
#include <mruby/internal.h>

#include <stdio.h>
#include <string.h>

// Defined in wasm.c
extern mrb_state *global_mrb;

// Forward declarations to avoid including mruby_compiler.h
typedef struct mrc_ccontext mrc_ccontext;
typedef struct mrc_irep mrc_irep;

mrc_ccontext* mrc_ccontext_new(mrb_state *mrb);
void mrc_ccontext_free(mrc_ccontext *cc);
mrc_irep* mrc_load_string_cxt(mrc_ccontext *cc, const uint8_t **source, size_t length);
void mrc_resolve_intern(mrc_ccontext *cc, mrc_irep *irep);

// Declared in task.h
MRB_API mrb_value mrb_execute_proc_synchronously(mrb_state *mrb, mrb_value proc, mrb_int argc, const mrb_value *argv);

// Static buffer for JSON output (avoid malloc/free for simplicity)
#define JSON_BUFFER_SIZE 65536
static char json_buffer[JSON_BUFFER_SIZE];

// Helper: Escape string for JSON
static void append_json_string(char **ptr, size_t *remaining, const char *str) {
  size_t len = strlen(str);
  char *p = *ptr;

  if (*remaining < len + 3) return;

  *p++ = '"';
  (*remaining)--;

  for (size_t i = 0; i < len && *remaining > 2; i++) {
    char c = str[i];
    if (c == '"' || c == '\\') {
      if (*remaining < 3) break;
      *p++ = '\\';
      (*remaining)--;
    }
    *p++ = c;
    (*remaining)--;
  }

  *p++ = '"';
  (*remaining)--;
  *p = '\0';

  *ptr = p;
}

// Get all global variables as JSON
EMSCRIPTEN_KEEPALIVE
const char* mrb_get_globals_json(void) {
  if (!global_mrb) {
    return "{\"error\":\"VM not initialized\"}";
  }

  // Get list of global variable names directly without using task system
  global_mrb->exc = NULL;
  mrb_value names_ary = mrb_f_global_variables(global_mrb, mrb_nil_value());

  if (global_mrb->exc || !mrb_array_p(names_ary)) {
    global_mrb->exc = NULL;
    return "{}";
  }

  // Build JSON manually
  char *p = json_buffer;
  size_t remaining = JSON_BUFFER_SIZE - 1;

  snprintf(p, remaining, "{");
  p += strlen(p);
  remaining = JSON_BUFFER_SIZE - (p - json_buffer) - 1;

  mrb_int len = RARRAY_LEN(names_ary);
  mrb_bool first = TRUE;

  for (mrb_int i = 0; i < len; i++) {
    mrb_value name_sym = mrb_ary_ref(global_mrb, names_ary, i);
    mrb_value name_str = mrb_sym_str(global_mrb, mrb_symbol(name_sym));
    const char *name = mrb_str_to_cstr(global_mrb, name_str);

    // Get value
    mrb_sym sym = mrb_intern_cstr(global_mrb, name);
    mrb_value val = mrb_gv_get(global_mrb, sym);

    // Skip internal variables without a valid class (e.g., _gc_root_)
    const char *classname = mrb_obj_classname(global_mrb, val);
    if (!classname) {
      continue; // Skip variables without a valid class
    }

    // Convert to string safely without using inspect to avoid potential infinite recursion
    const char *val_cstr;
    char fallback_buf[128];

    global_mrb->exc = NULL;
    mrb_value val_str = mrb_inspect(global_mrb, val);
    if (global_mrb->exc) {
      global_mrb->exc = NULL;
      // Fallback: show class name and object ID instead
      snprintf(fallback_buf, sizeof(fallback_buf), "#<%s:0x%p>",
               classname, (void*)mrb_obj_id(val));
      val_cstr = fallback_buf;
    } else {
      val_cstr = mrb_str_to_cstr(global_mrb, val_str);
    }

    if (!first && remaining > 2) {
      *p++ = ',';
      remaining--;
    }
    first = FALSE;

    append_json_string(&p, &remaining, name);
    if (remaining > 2) {
      *p++ = ':';
      remaining--;
    }

    append_json_string(&p, &remaining, val_cstr);

    remaining = JSON_BUFFER_SIZE - (p - json_buffer) - 1;
  }

  if (remaining > 2) {
    *p++ = '}';
    *p = '\0';
  }

  return json_buffer;
}

// Get component debug info - using mrb_eval_string since mrb_funcall doesn't work in WASM
EMSCRIPTEN_KEEPALIVE
const char* mrb_get_component_debug_info(const char* method) {
  if (!global_mrb) {
    return "{\"error\":\"VM not initialized\"}";
  }

  // Build Ruby code to call the method
  static char code_buffer[256];
  snprintf(code_buffer, sizeof(code_buffer), "$__funicular_debug__.%s", method);

  // Compile the code
  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  if (!cc) {
    return "{\"error\":\"Failed to create compiler context\"}";
  }

  const uint8_t *script = (const uint8_t *)code_buffer;
  size_t size = strlen(code_buffer);
  mrc_irep *irep = mrc_load_string_cxt(cc, &script, size);

  if (!irep) {
    mrc_ccontext_free(cc);
    return "{\"error\":\"Syntax error\"}";
  }

  // Create proc
  mrc_resolve_intern(cc, irep);
  struct RProc *proc = mrb_proc_new(global_mrb, (const mrb_irep *)irep);
  proc->e.target_class = global_mrb->object_class;

  mrc_ccontext_free(cc);

  // Execute synchronously
  mrb_value proc_val = mrb_obj_value(proc);
  global_mrb->exc = NULL;
  mrb_value result = mrb_execute_proc_synchronously(global_mrb, proc_val, 0, NULL);

  // Check for exception
  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      global_mrb->exc = NULL;
      return "{\"error\":\"Exception occurred\"}";
    }
    static char error_buffer[JSON_BUFFER_SIZE];
    const char *exc_cstr = mrb_str_to_cstr(global_mrb, exc_str);
    snprintf(error_buffer, JSON_BUFFER_SIZE, "{\"error\":\"%s\"}", exc_cstr);
    return error_buffer;
  }

  // Result should be a JSON string
  if (!mrb_string_p(result)) {
    const char *type_name = mrb_obj_classname(global_mrb, result);
    static char type_error_buffer[256];
    snprintf(type_error_buffer, 256, "{\"error\":\"Invalid result type: %s\"}", type_name ? type_name : "unknown");
    return type_error_buffer;
  }

  // Return the string directly
  static char result_buffer[JSON_BUFFER_SIZE];
  const char *result_str = mrb_str_to_cstr(global_mrb, result);
  snprintf(result_buffer, JSON_BUFFER_SIZE, "{\"result\":%s}", result_str);
  return result_buffer;
}

// Get component state by ID - using mrb_eval_string since mrb_funcall doesn't work in WASM
EMSCRIPTEN_KEEPALIVE
const char* mrb_get_component_state_by_id(int component_id) {
  if (!global_mrb) {
    return "{\"error\":\"VM not initialized\"}";
  }

  // Build Ruby code to get both state and ivars
  static char code_buffer[512];
  snprintf(code_buffer, sizeof(code_buffer),
    "state = $__funicular_debug__.get_component_state(%d); "
    "ivars = $__funicular_debug__.get_component_instance_variables(%d); "
    "\"{\\\"state\\\":#{state},\\\"ivars\\\":#{ivars}}\"",
    component_id, component_id);

  // Compile the code
  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  if (!cc) {
    return "{\"error\":\"Failed to create compiler context\"}";
  }

  const uint8_t *script = (const uint8_t *)code_buffer;
  size_t size = strlen(code_buffer);
  mrc_irep *irep = mrc_load_string_cxt(cc, &script, size);

  if (!irep) {
    mrc_ccontext_free(cc);
    return "{\"error\":\"Syntax error\"}";
  }

  // Create proc
  mrc_resolve_intern(cc, irep);
  struct RProc *proc = mrb_proc_new(global_mrb, (const mrb_irep *)irep);
  proc->e.target_class = global_mrb->object_class;

  mrc_ccontext_free(cc);

  // Execute synchronously
  mrb_value proc_val = mrb_obj_value(proc);
  global_mrb->exc = NULL;
  mrb_value result = mrb_execute_proc_synchronously(global_mrb, proc_val, 0, NULL);

  // Check for exception
  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      global_mrb->exc = NULL;
      return "{\"error\":\"Exception occurred\"}";
    }
    static char error_buffer[JSON_BUFFER_SIZE];
    const char *exc_cstr = mrb_str_to_cstr(global_mrb, exc_str);
    snprintf(error_buffer, JSON_BUFFER_SIZE, "{\"error\":\"%s\"}", exc_cstr);
    return error_buffer;
  }

  // Result should be a JSON string
  if (!mrb_string_p(result)) {
    const char *type_name = mrb_obj_classname(global_mrb, result);
    static char type_error_buffer[256];
    snprintf(type_error_buffer, 256, "{\"error\":\"Invalid result type: %s\"}", type_name ? type_name : "unknown");
    return type_error_buffer;
  }

  // Return the result directly wrapped
  static char result_buffer[JSON_BUFFER_SIZE];
  const char *result_str = mrb_str_to_cstr(global_mrb, result);
  snprintf(result_buffer, JSON_BUFFER_SIZE, "{\"result\":%s}", result_str);
  return result_buffer;
}

// Evaluate Ruby code synchronously
EMSCRIPTEN_KEEPALIVE
const char* mrb_eval_string(const char* code) {
  if (!global_mrb) {
    return "{\"error\":\"VM not initialized\"}";
  }

  if (!code || strlen(code) == 0) {
    return "{\"error\":\"Empty code\"}";
  }

  global_mrb->exc = NULL;

  // Compile the code
  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  if (!cc) {
    return "{\"error\":\"Failed to create compiler context\"}";
  }

  const uint8_t *script = (const uint8_t *)code;
  size_t size = strlen(code);
  mrc_irep *irep = mrc_load_string_cxt(cc, &script, size);

  if (!irep) {
    mrc_ccontext_free(cc);
    return "{\"error\":\"Syntax error\"}";
  }

  // Create proc
  mrc_resolve_intern(cc, irep);
  struct RProc *proc = mrb_proc_new(global_mrb, (const mrb_irep *)irep);
  proc->e.target_class = global_mrb->object_class;

  mrc_ccontext_free(cc);

  // Execute synchronously
  mrb_value proc_val = mrb_obj_value(proc);
  mrb_value result = mrb_execute_proc_synchronously(global_mrb, proc_val, 0, NULL);

  // Check for exception
  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;

    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      global_mrb->exc = NULL;
      return "{\"error\":\"Exception occurred (failed to inspect)\"}";
    }
    const char *exc_cstr = mrb_str_to_cstr(global_mrb, exc_str);

    char *p = json_buffer;
    size_t remaining = JSON_BUFFER_SIZE - 1;

    snprintf(p, remaining, "{\"error\":");
    p += strlen(p);
    remaining = JSON_BUFFER_SIZE - (p - json_buffer) - 1;

    append_json_string(&p, &remaining, exc_cstr);

    if (remaining > 2) {
      *p++ = '}';
      *p = '\0';
    }

    return json_buffer;
  }

  // Return result
  mrb_value result_str = mrb_inspect(global_mrb, result);
  if (global_mrb->exc) {
    global_mrb->exc = NULL;
    return "{\"error\":\"Failed to inspect result\"}";
  }
  const char *result_cstr = mrb_str_to_cstr(global_mrb, result_str);

  char *p = json_buffer;
  size_t remaining = JSON_BUFFER_SIZE - 1;

  snprintf(p, remaining, "{\"result\":");
  p += strlen(p);
  remaining = JSON_BUFFER_SIZE - (p - json_buffer) - 1;

  append_json_string(&p, &remaining, result_cstr);

  if (remaining > 2) {
    *p++ = '}';
    *p = '\0';
  }

  return json_buffer;
}
