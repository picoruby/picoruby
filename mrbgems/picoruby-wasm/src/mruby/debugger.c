// Debug API for PicoRuby WASM DevTools
// This file provides debugging functions for browser DevTools extension

#include <emscripten.h>
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/proc.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/debug.h>
#include <mruby/internal.h>
#include <mruby/presym.h>
#include <mruby/opcode.h>
#include <mruby/irep.h>
#include "task.h"

#include <stdio.h>
#include <string.h>
#include <stddef.h>

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

// Debug state machine
typedef enum {
  WASM_DBG_IDLE = 0,
  WASM_DBG_PAUSED,
  WASM_DBG_RUN,
  WASM_DBG_STEP,
  WASM_DBG_NEXT,
} wasm_dbg_mode;

typedef struct wasm_debug_state {
  wasm_dbg_mode mode;
  mrb_value paused_task;     /* Task object that is suspended */
  mrb_value binding;         /* Captured Binding object */
  const char *pause_file;
  int32_t pause_line;
  int32_t pause_id;          /* Monotonic counter; incremented on each pause */
  /* For step/next tracking */
  const char *prev_file;
  int32_t prev_line;
  mrb_callinfo *prev_ci;
  mrb_bool hook_active;
  mrb_bool eval_in_progress;
} wasm_debug_state;

static wasm_debug_state g_dbg = {0};

/* Get current task from mrb->c via pointer arithmetic */
#define MRB2TASK(mrb) \
  ((mrb_task *)((uint8_t *)(mrb)->c - offsetof(mrb_task, c)))

/* Forward declaration - defined in mruby-binding */
extern const struct RProc *
mrb_binding_extract_proc(mrb_state *mrb, mrb_value binding);

/*
 * Helper: compile and execute Ruby code synchronously.
 * Returns mrb_undef_value() on compilation failure.
 * Caller must check mrb->exc for runtime errors.
 */
static mrb_value
debug_eval_code(mrb_state *mrb, const char *code, size_t len)
{
  mrc_ccontext *cc = mrc_ccontext_new(mrb);
  if (!cc) return mrb_undef_value();

  const uint8_t *script = (const uint8_t *)code;
  mrc_irep *irep = mrc_load_string_cxt(cc, &script, len);
  if (!irep) {
    mrc_ccontext_free(cc);
    return mrb_undef_value();
  }

  mrc_resolve_intern(cc, irep);
  struct RProc *proc = mrb_proc_new(mrb, (const mrb_irep *)irep);
  proc->e.target_class = mrb->object_class;
  mrc_ccontext_free(cc);

  mrb->exc = NULL;
  mrb_value proc_val = mrb_obj_value(proc);
  return mrb_execute_proc_synchronously(mrb, proc_val, 0, NULL);
}

/*
 * Binding#irb - Pause execution and enter debug mode.
 * The current task is suspended; JS polls mrb_debug_get_status()
 * to detect the paused state and interact with the debugger.
 */
static mrb_value
mrb_binding_irb(mrb_state *mrb, mrb_value self)
{
  fprintf(stderr, "[binding.irb] CALLED  mode=%d eval_in_progress=%d root_c=%d\n",
          g_dbg.mode, g_dbg.eval_in_progress, (mrb->c == mrb->root_c));

  /* Must be called from task context (not root) */
  if (mrb->c == mrb->root_c) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "binding.irb requires task context");
  }

  /* Ignore nested calls while already paused */
  if (g_dbg.mode == WASM_DBG_PAUSED) {
    fprintf(stderr, "[binding.irb] SKIPPED (already paused)\n");
    return mrb_nil_value();
  }

  /* Store binding and protect from GC */
  g_dbg.binding = self;
  mrb_gc_register(mrb, self);

  /* Extract source location from binding for display */
  g_dbg.pause_file = NULL;
  g_dbg.pause_line = 0;
  mrb_value pc_val = mrb_iv_get(mrb, self, MRB_SYM(pc));
  if (!mrb_nil_p(pc_val)) {
    const struct RProc *proc = mrb_binding_extract_proc(mrb, self);
    if (proc && proc->upper && !MRB_PROC_CFUNC_P(proc->upper)) {
      const mrb_irep *irep = proc->upper->body.irep;
      mrb_int pc = mrb_integer(pc_val);
      mrb_debug_get_position(mrb, irep, (uint32_t)pc,
                             &g_dbg.pause_line, &g_dbg.pause_file);
    }
  }

  /* Escape all on-stack REnvs in the binding's proc chain to the heap.
   *
   * The binding's proc is an lvspace wrapper whose e.env is a stack-allocated
   * REnv pointing into the current task's register file (stbase).  After
   * mrb_suspend_task() the task is frozen, but when mrb_execute_proc_
   * synchronously() later accesses the binding from a different task context,
   * mrb->c differs from env->cxt.  Calling mrb_env_unshare() while we are
   * still on the same task copies the register values to the heap and marks
   * the env as closed (cxt = NULL), ensuring local_variable_get() reads
   * stable heap memory regardless of which task is active. */
  {
    const struct RProc *p = mrb_binding_extract_proc(mrb, self);
    while (p && !MRB_PROC_CFUNC_P(p)) {
      struct REnv *e = MRB_PROC_ENV(p);
      if (e && MRB_ENV_ONSTACK_P(e)) {
        mrb_env_unshare(mrb, e, FALSE);
      }
      if (MRB_PROC_SCOPE_P(p)) break;
      p = p->upper;
    }
  }

  /* Get current task via pointer arithmetic on mrb->c */
  mrb_task *t = MRB2TASK(mrb);
  g_dbg.paused_task = t->self;
  mrb_gc_register(mrb, t->self);

  /* Enter paused mode */
  g_dbg.mode = WASM_DBG_PAUSED;
  g_dbg.pause_id++;

  fprintf(stderr, "[binding.irb] PAUSING at %s:%d  pause_id=%d  task=%p\n",
          g_dbg.pause_file ? g_dbg.pause_file : "(unknown)",
          g_dbg.pause_line, g_dbg.pause_id,
          (void *)mrb_ptr(g_dbg.paused_task));

  /* Suspend the task - sets switching=TRUE, VM will exit */
  mrb_suspend_task(mrb, g_dbg.paused_task);

  fprintf(stderr, "[binding.irb] RESUMED (back from suspend)  mode=%d\n",
          g_dbg.mode);
  /* Execution continues here after mrb_debug_continue() resumes the task */
  return mrb_nil_value();
}

/*
 * Initialize the WASM debugger.
 * Called from picorb_init() when PICORUBY_DEBUG is defined.
 */
void
mrb_wasm_debugger_init(mrb_state *mrb)
{
  /* Initialize debug state */
  memset(&g_dbg, 0, sizeof(g_dbg));
  g_dbg.mode = WASM_DBG_IDLE;
  g_dbg.binding = mrb_nil_value();
  g_dbg.paused_task = mrb_nil_value();

  /* Define Binding#irb method */
  struct RClass *binding_class = mrb_class_get(mrb, "Binding");
  mrb_define_method(mrb, binding_class, "irb",
                    mrb_binding_irb, MRB_ARGS_NONE());
}

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
      snprintf(fallback_buf, sizeof(fallback_buf), "#<%s:0x%lx>",
               classname, (unsigned long)mrb_obj_id(val));
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
    char *p = error_buffer;
    size_t rem = JSON_BUFFER_SIZE;
    size_t n = snprintf(p, rem, "{\"error\":");
    p += n; rem -= n;
    append_json_string(&p, &rem, exc_cstr);
    snprintf(p, rem, "}");
    return error_buffer;
  }

  // Result should be a JSON string
  if (!mrb_string_p(result)) {
    const char *type_name = mrb_obj_classname(global_mrb, result);
    static char type_error_buffer[256];
    char *p = type_error_buffer;
    size_t rem = sizeof(type_error_buffer);
    size_t n = snprintf(p, rem, "{\"error\":");
    p += n; rem -= n;
    append_json_string(&p, &rem, type_name ? type_name : "unknown");
    snprintf(p, rem, "}");
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
    char *p = error_buffer;
    size_t rem = JSON_BUFFER_SIZE;
    size_t n = snprintf(p, rem, "{\"error\":");
    p += n; rem -= n;
    append_json_string(&p, &rem, exc_cstr);
    snprintf(p, rem, "}");
    return error_buffer;
  }

  // Result should be a JSON string
  if (!mrb_string_p(result)) {
    const char *type_name = mrb_obj_classname(global_mrb, result);
    static char type_error_buffer[256];
    char *p = type_error_buffer;
    size_t rem = sizeof(type_error_buffer);
    size_t n = snprintf(p, rem, "{\"error\":");
    p += n; rem -= n;
    append_json_string(&p, &rem, type_name ? type_name : "unknown");
    snprintf(p, rem, "}");
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

// ---------------------------------------------------------------------------
// Debug API: status, continue, locals, eval, step, next, callstack
// ---------------------------------------------------------------------------

// Helper: build a JSON error response into a static buffer
static const char *
debug_json_error(const char *msg)
{
  static char err_buf[512];
  char *p = err_buf;
  size_t remaining = sizeof(err_buf) - 1;

  snprintf(p, remaining, "{\"error\":");
  p += strlen(p);
  remaining = sizeof(err_buf) - (p - err_buf) - 1;
  append_json_string(&p, &remaining, msg);
  if (remaining > 2) {
    *p++ = '}';
    *p = '\0';
  }
  return err_buf;
}

// Polled by JS to detect pause state (every ~200ms)
EMSCRIPTEN_KEEPALIVE
const char* mrb_debug_get_status(void)
{
  static char status_buf[512];

  switch (g_dbg.mode) {
    case WASM_DBG_PAUSED: {
      char *p = status_buf;
      size_t remaining = sizeof(status_buf) - 1;

      snprintf(p, remaining, "{\"mode\":\"paused\",\"file\":");
      p += strlen(p);
      remaining = sizeof(status_buf) - (p - status_buf) - 1;

      append_json_string(&p, &remaining,
                         g_dbg.pause_file ? g_dbg.pause_file : "(unknown)");
      if (remaining > 64) {
        int n = snprintf(p, remaining, ",\"line\":%d,\"pause_id\":%d}",
                         g_dbg.pause_line, g_dbg.pause_id);
        p += n;
      }
      return status_buf;
    }
    case WASM_DBG_STEP:
      return "{\"mode\":\"stepping\"}";
    case WASM_DBG_NEXT:
      return "{\"mode\":\"stepping\"}";
    case WASM_DBG_RUN:
      return "{\"mode\":\"running\"}";
    default:
      return "{\"mode\":\"idle\"}";
  }
}

// Resume execution after a pause
EMSCRIPTEN_KEEPALIVE
const char* mrb_debug_continue(void)
{
  fprintf(stderr, "[continue] ENTER  mode=%d\n", g_dbg.mode);

  if (!global_mrb || g_dbg.mode != WASM_DBG_PAUSED) {
    fprintf(stderr, "[continue] ERROR not paused\n");
    return debug_json_error("not paused");
  }

  /* Clean up GC roots */
  if (!mrb_nil_p(g_dbg.binding)) {
    mrb_gc_unregister(global_mrb, g_dbg.binding);
  }

  mrb_value task = g_dbg.paused_task;
  fprintf(stderr, "[continue] task=%p  task_nil=%d\n",
          mrb_nil_p(task) ? NULL : (void *)mrb_ptr(task),
          mrb_nil_p(task));

  /* Reset debug state before resuming */
  g_dbg.mode = WASM_DBG_IDLE;
  g_dbg.binding = mrb_nil_value();
  g_dbg.paused_task = mrb_nil_value();
  g_dbg.pause_file = NULL;
  g_dbg.pause_line = 0;

#ifdef MRB_USE_DEBUG_HOOK
  /* Remove hook unless breakpoints are active */
  if (!g_dbg.hook_active) {
    global_mrb->code_fetch_hook = NULL;
  }
#endif

  /* Resume the suspended task.
   * Unregister the GC root BEFORE resume so that if the task
   * synchronously re-pauses (hitting another binding.irb), the
   * re-registration inside mrb_binding_irb is not cancelled out. */
  if (!mrb_nil_p(task)) {
    mrb_gc_unregister(global_mrb, task);
    fprintf(stderr, "[continue] calling mrb_resume_task...\n");
    mrb_resume_task(global_mrb, task);
    fprintf(stderr, "[continue] mrb_resume_task returned  mode=%d\n",
            g_dbg.mode);
  } else {
    fprintf(stderr, "[continue] WARNING task is nil, cannot resume\n");
  }

  /* If the task synchronously hit another binding.irb, report the
   * new paused state directly so the JS side doesn't need to poll. */
  if (g_dbg.mode == WASM_DBG_PAUSED) {
    fprintf(stderr, "[continue] RE-PAUSED at %s:%d  pause_id=%d\n",
            g_dbg.pause_file ? g_dbg.pause_file : "(unknown)",
            g_dbg.pause_line, g_dbg.pause_id);
    return mrb_debug_get_status();
  }

  fprintf(stderr, "[continue] NOT re-paused, returning running  mode=%d\n",
          g_dbg.mode);
  return "{\"status\":\"running\"}";
}

// Get local variables from the captured binding as JSON
EMSCRIPTEN_KEEPALIVE
const char* mrb_debug_get_locals(void)
{
  if (!global_mrb || g_dbg.mode != WASM_DBG_PAUSED) {
    return debug_json_error("not paused");
  }
  if (mrb_nil_p(g_dbg.binding)) {
    return debug_json_error("no binding captured");
  }

  /* Store binding in a global variable for Ruby access */
  mrb_gv_set(global_mrb,
             mrb_intern_lit(global_mrb, "$__debug_binding__"),
             g_dbg.binding);

  /* Execute Ruby code to extract locals as a flat array:
   * [name1_str, val1_inspect, name2_str, val2_inspect, ...] */
  static const char *code =
    "__b__=$__debug_binding__;"
    "__r__=[];"
    "__b__.local_variables.each{|__v__|"
    "  __r__ << __v__.to_s;"
    "  __r__ << __b__.local_variable_get(__v__).inspect"
    "};"
    "__r__ << \"__self__\";"
    "__r__ << __b__.receiver.inspect;"
    "__r__ << \"__source__\";"
    "__r__ << __b__.source_location.inspect;"
    "__r__";

  global_mrb->exc = NULL;
  mrb_value result = debug_eval_code(global_mrb, code, strlen(code));

  if (global_mrb->exc || mrb_undef_p(result) || !mrb_array_p(result)) {
    global_mrb->exc = NULL;
    return debug_json_error("failed to get locals");
  }

  /* Build JSON object from flat array of [name, value, ...] pairs */
  char *p = json_buffer;
  size_t remaining = JSON_BUFFER_SIZE - 1;
  *p++ = '{';
  remaining--;

  mrb_int len = RARRAY_LEN(result);
  mrb_bool first = TRUE;

  for (mrb_int i = 0; i + 1 < len; i += 2) {
    mrb_value name = mrb_ary_ref(global_mrb, result, i);
    mrb_value val = mrb_ary_ref(global_mrb, result, i + 1);

    const char *name_cstr = mrb_string_p(name)
      ? mrb_str_to_cstr(global_mrb, name) : "?";
    const char *val_cstr = mrb_string_p(val)
      ? mrb_str_to_cstr(global_mrb, val) : "nil";

    if (!first && remaining > 2) {
      *p++ = ',';
      remaining--;
    }
    first = FALSE;

    append_json_string(&p, &remaining, name_cstr);
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

// Evaluate code in the paused binding context
EMSCRIPTEN_KEEPALIVE
const char* mrb_debug_eval_in_binding(const char* code)
{
  if (!global_mrb || g_dbg.mode != WASM_DBG_PAUSED) {
    return debug_json_error("not paused");
  }
  if (!code || !*code) {
    return debug_json_error("empty code");
  }
  if (mrb_nil_p(g_dbg.binding)) {
    return debug_json_error("no binding captured");
  }

  g_dbg.eval_in_progress = TRUE;

#ifdef MRB_USE_DEBUG_HOOK
  /* Disable hook during eval to prevent recursion */
  void (*saved_hook)(struct mrb_state*, const struct mrb_irep*,
                     const mrb_code*, mrb_value*) =
    global_mrb->code_fetch_hook;
  global_mrb->code_fetch_hook = NULL;
#endif

  /* Store binding in global for Ruby access */
  mrb_gv_set(global_mrb,
             mrb_intern_lit(global_mrb, "$__debug_binding__"),
             g_dbg.binding);

  /* Step 1: Get local variable names */
  static const char *get_vars_code =
    "$__debug_binding__.local_variables";
  global_mrb->exc = NULL;
  mrb_value vars_ary = debug_eval_code(global_mrb,
                                       get_vars_code,
                                       strlen(get_vars_code));

  mrb_int num_vars = 0;
  if (!global_mrb->exc && mrb_array_p(vars_ary)) {
    num_vars = RARRAY_LEN(vars_ary);
  }
  global_mrb->exc = NULL;

  /* Debug: verify local_variable_get returns the correct type */
  {
    static char lv_test_buf[256];
    snprintf(lv_test_buf, sizeof(lv_test_buf),
             "$__debug_binding__.local_variables.map{|v|"
             "[v.to_s,$__debug_binding__.local_variable_get(v).class.name]}");
    mrb_value lv_test = debug_eval_code(global_mrb, lv_test_buf,
                                        strlen(lv_test_buf));
    global_mrb->exc = NULL;
    if (mrb_array_p(lv_test)) {
      fprintf(stderr, "[dbg] variable types:\n");
      for (mrb_int i = 0; i < RARRAY_LEN(lv_test); i++) {
        mrb_value pair = mrb_ary_ref(global_mrb, lv_test, i);
        if (mrb_array_p(pair) && RARRAY_LEN(pair) >= 2) {
          mrb_value nm = mrb_ary_ref(global_mrb, pair, 0);
          mrb_value tp = mrb_ary_ref(global_mrb, pair, 1);
          const char *ns = mrb_string_p(nm) ? mrb_str_to_cstr(global_mrb, nm) : "?";
          const char *ts = mrb_string_p(tp) ? mrb_str_to_cstr(global_mrb, tp) : "?";
          fprintf(stderr, "  %s => %s\n", ns, ts);
        }
      }
    } else {
      fprintf(stderr, "[dbg] lv_test type=%d\n", mrb_type(lv_test));
    }
  }

  /* Step 2: Build wrapper code with variable pre-loading and write-back.
   * Use newlines as statement separators to avoid any parser ambiguity.
   * Format:
   *   __b__=$__debug_binding__
   *   var1=__b__.local_variable_get(:var1)
   *   ...
   *   __r__=(USER_CODE)
   *   __b__.local_variable_set(:var1, var1)
   *   ...
   *   __r__
   *
   * Write-back propagates side effects (e.g. assignments) to the
   * original binding so that subsequent evals see updated values.
   */
  size_t user_len = strlen(code);
  size_t buf_size = user_len + (size_t)num_vars * 160 + 512;
  char *wrapper = (char *)mrb_malloc_simple(global_mrb, buf_size);
  if (!wrapper) {
    g_dbg.eval_in_progress = FALSE;
#ifdef MRB_USE_DEBUG_HOOK
    global_mrb->code_fetch_hook = saved_hook;
#endif
    return debug_json_error("out of memory");
  }

  char *wp = wrapper;
  size_t wr = buf_size;
  int wn;

  wn = snprintf(wp, wr, "__b__=$__debug_binding__\n");
  wp += wn; wr -= (size_t)wn;

  /* Pre-load each local variable */
  for (mrb_int i = 0; i < num_vars && wr > 64; i++) {
    mrb_value sym = mrb_ary_ref(global_mrb, vars_ary, i);
    if (!mrb_symbol_p(sym)) continue;
    const char *vname = mrb_sym_name(global_mrb, mrb_symbol(sym));
    if (!vname || !*vname) continue;
    wn = snprintf(wp, wr, "%s=__b__.local_variable_get(:%s)\n",
                  vname, vname);
    wp += wn; wr -= (size_t)wn;
  }

  /* User code - capture return value */
  wn = snprintf(wp, wr, "__r__=(%s)\n", code);
  wp += wn; wr -= (size_t)wn;

  /* Write back each local variable to the binding */
  for (mrb_int i = 0; i < num_vars && wr > 64; i++) {
    mrb_value sym = mrb_ary_ref(global_mrb, vars_ary, i);
    if (!mrb_symbol_p(sym)) continue;
    const char *vname = mrb_sym_name(global_mrb, mrb_symbol(sym));
    if (!vname || !*vname) continue;
    wn = snprintf(wp, wr, "__b__.local_variable_set(:%s,%s)\n",
                  vname, vname);
    wp += wn; wr -= (size_t)wn;
  }

  /* Return the captured result */
  wn = snprintf(wp, wr, "__r__");
  wp += wn;

  /* Step 3: Compile and execute the wrapper */
  global_mrb->exc = NULL;
  mrb_value result = debug_eval_code(global_mrb, wrapper,
                                     (size_t)(wp - wrapper));
  mrb_free(global_mrb, wrapper);

  g_dbg.eval_in_progress = FALSE;
#ifdef MRB_USE_DEBUG_HOOK
  global_mrb->code_fetch_hook = saved_hook;
#endif

  /* Handle compile error */
  if (mrb_undef_p(result) && !global_mrb->exc) {
    return debug_json_error("syntax error");
  }

  /* Handle runtime exception */
  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;

    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      global_mrb->exc = NULL;
      return debug_json_error("exception (failed to inspect)");
    }
    const char *exc_cstr = mrb_str_to_cstr(global_mrb, exc_str);

    char *ep = json_buffer;
    size_t eremaining = JSON_BUFFER_SIZE - 1;
    snprintf(ep, eremaining, "{\"error\":");
    ep += strlen(ep);
    eremaining = JSON_BUFFER_SIZE - (ep - json_buffer) - 1;
    append_json_string(&ep, &eremaining, exc_cstr);
    if (eremaining > 2) { *ep++ = '}'; *ep = '\0'; }
    return json_buffer;
  }

  fprintf(stderr, "[dbg] result mrb_type=%d classname=%s\n",
          mrb_type(result),
          mrb_obj_classname(global_mrb, result) ?
            mrb_obj_classname(global_mrb, result) : "?");

  /* Return inspected result */
  mrb_value result_str = mrb_inspect(global_mrb, result);
  if (global_mrb->exc) {
    global_mrb->exc = NULL;
    return debug_json_error("failed to inspect result");
  }
  const char *result_cstr = mrb_str_to_cstr(global_mrb, result_str);

  char *rp = json_buffer;
  size_t rremaining = JSON_BUFFER_SIZE - 1;
  snprintf(rp, rremaining, "{\"result\":");
  rp += strlen(rp);
  rremaining = JSON_BUFFER_SIZE - (rp - json_buffer) - 1;
  append_json_string(&rp, &rremaining, result_cstr);
  if (rremaining > 2) { *rp++ = '}'; *rp = '\0'; }
  return json_buffer;
}

// ---------------------------------------------------------------------------
// Phase 2: code_fetch_hook, step, next, callstack
// ---------------------------------------------------------------------------

#ifdef MRB_USE_DEBUG_HOOK

/*
 * Create a Binding object from the code_fetch_hook context.
 * Replicates the lvspace wrapping from mruby-binding so that
 * local_variables / local_variable_get / source_location all work.
 */
static mrb_value
debug_create_binding(mrb_state *mrb, const mrb_irep *irep,
                     const mrb_code *pc, mrb_value *regs)
{
  mrb_callinfo *ci = mrb->c->ci;
  const struct RProc *proc = ci->proc;

  if (!proc || MRB_PROC_CFUNC_P(proc)) {
    return mrb_nil_value();
  }

  /* Ensure env exists for the current frame */
  struct REnv *env = mrb_vm_ci_env(ci);
  if (!env) {
    env = mrb_env_new(mrb, mrb->c, ci, proc->body.irep->nlocals,
                      ci->stack, mrb_vm_ci_target_class(ci));
    ci->u.env = env;
  }

  /* Create binding object */
  struct RObject *binding = MRB_OBJ_ALLOC(mrb, MRB_TT_OBJECT,
    mrb_class_get_id(mrb, MRB_SYM(Binding)));

  /* Store PC offset */
  uint32_t pc_offset = (uint32_t)(pc - irep->iseq);
  mrb_obj_iv_set(mrb, binding, MRB_SYM(pc),
                 mrb_fixnum_value(pc_offset));

  /* Create lvspace proc wrapping the actual proc.
   * This mirrors binding_wrap_lvspace from mruby-binding. */

  /* 1. Dummy irep for lvspace */
  static const mrb_code iseq_dummy[] = { OP_RETURN, 0 };
  mrb_irep *lv_irep = mrb_add_irep(mrb);
  lv_irep->flags = MRB_ISEQ_NO_FREE;
  lv_irep->iseq = iseq_dummy;
  lv_irep->ilen = sizeof(iseq_dummy) / sizeof(iseq_dummy[0]);
  lv_irep->lv = NULL;
  lv_irep->nlocals = 1;
  lv_irep->nregs = 1;

  /* 2. Lvspace proc: upper = actual proc */
  struct RProc *lvspace = MRB_OBJ_ALLOC(mrb, MRB_TT_PROC,
                                        mrb->proc_class);
  lvspace->body.irep = lv_irep;
  lvspace->upper = proc;
  if (env && env->tt == MRB_TT_ENV) {
    lvspace->e.env = env;
    lvspace->flags |= MRB_PROC_ENVSET;
  }

  /* 3. Lvspace env */
  struct REnv *lv_env = MRB_OBJ_ALLOC(mrb, MRB_TT_ENV, NULL);
  mrb_value *stacks = (mrb_value *)mrb_calloc(mrb, 1, sizeof(mrb_value));
  lv_env->mid = 0;
  lv_env->stack = stacks;
  if (env && env->stack && MRB_ENV_LEN(env) > 0) {
    lv_env->stack[0] = env->stack[0];
  }
  else {
    lv_env->stack[0] = mrb_nil_value();
  }
  MRB_ENV_SET_LEN(lv_env, 1);

  /* Set binding ivars */
  mrb_obj_iv_set(mrb, binding, MRB_SYM(proc),
                 mrb_obj_value(lvspace));
  mrb_obj_iv_set(mrb, binding, MRB_SYM(recv), regs[0]);
  mrb_obj_iv_set(mrb, binding, MRB_SYM(env),
                 mrb_obj_value(lv_env));

  return mrb_obj_value(binding);
}

/*
 * code_fetch_hook - called by VM at each bytecode instruction
 * when MRB_USE_DEBUG_HOOK is enabled and the hook is installed.
 */
static void
wasm_code_fetch_hook(mrb_state *mrb, const mrb_irep *irep,
                     const mrb_code *pc, mrb_value *regs)
{
  /* Skip during eval to prevent recursion */
  if (g_dbg.eval_in_progress) return;

  /* Only active in STEP or NEXT mode */
  if (g_dbg.mode != WASM_DBG_STEP && g_dbg.mode != WASM_DBG_NEXT) {
    return;
  }

  /* Get current file and line */
  uint32_t pc_offset = (uint32_t)(pc - irep->iseq);
  const char *file = mrb_debug_get_filename(mrb, irep, pc_offset);
  int32_t line = mrb_debug_get_line(mrb, irep, pc_offset);

  /* Skip if no debug info available */
  if (!file || line <= 0) return;

  /* Skip if same file and line (no line change) */
  if (g_dbg.prev_file == file && g_dbg.prev_line == line) {
    return;
  }

  /* NEXT mode: skip if we descended into a deeper call frame */
  if (g_dbg.mode == WASM_DBG_NEXT) {
    if ((intptr_t)(g_dbg.prev_ci) < (intptr_t)(mrb->c->ci)) {
      return;
    }
  }

  /* We have a line change at the right depth - break here */
  g_dbg.prev_file = file;
  g_dbg.prev_line = line;
  g_dbg.pause_file = file;
  g_dbg.pause_line = line;

  /* Clean up previous binding if any */
  if (!mrb_nil_p(g_dbg.binding)) {
    mrb_gc_unregister(mrb, g_dbg.binding);
  }

  /* Capture binding from current execution context */
  g_dbg.binding = debug_create_binding(mrb, irep, pc, regs);
  if (!mrb_nil_p(g_dbg.binding)) {
    mrb_gc_register(mrb, g_dbg.binding);
  }

  /* Get current task and suspend it */
  if (mrb->c != mrb->root_c) {
    mrb_task *t = MRB2TASK(mrb);
    if (mrb_nil_p(g_dbg.paused_task)) {
      g_dbg.paused_task = t->self;
      mrb_gc_register(mrb, t->self);
    }
  }

  g_dbg.mode = WASM_DBG_PAUSED;

  fprintf(stderr, "[debugger] Step paused at %s:%d\n",
          file, line);

  /* Suspend the task - VM will exit */
  if (!mrb_nil_p(g_dbg.paused_task)) {
    mrb_suspend_task(mrb, g_dbg.paused_task);
  }
}

#endif /* MRB_USE_DEBUG_HOOK */

// Step: break on next line change (any depth)
EMSCRIPTEN_KEEPALIVE
const char* mrb_debug_step(void)
{
#ifdef MRB_USE_DEBUG_HOOK
  if (!global_mrb || g_dbg.mode != WASM_DBG_PAUSED) {
    return debug_json_error("not paused");
  }

  /* Record current position for change detection */
  g_dbg.prev_file = g_dbg.pause_file;
  g_dbg.prev_line = g_dbg.pause_line;

  /* Clean up current binding (new one will be captured in hook) */
  if (!mrb_nil_p(g_dbg.binding)) {
    mrb_gc_unregister(global_mrb, g_dbg.binding);
    g_dbg.binding = mrb_nil_value();
  }

  /* Set step mode and install hook */
  g_dbg.mode = WASM_DBG_STEP;
  global_mrb->code_fetch_hook = wasm_code_fetch_hook;

  /* Resume the suspended task (unregister before resume; see continue) */
  mrb_value task = g_dbg.paused_task;
  g_dbg.paused_task = mrb_nil_value();
  if (!mrb_nil_p(task)) {
    mrb_gc_unregister(global_mrb, task);
    mrb_resume_task(global_mrb, task);
  }

  if (g_dbg.mode == WASM_DBG_PAUSED) {
    return mrb_debug_get_status();
  }
  return "{\"status\":\"stepping\"}";
#else
  return debug_json_error("debug hook not available");
#endif
}

// Next: break on next line change at same or shallower depth
EMSCRIPTEN_KEEPALIVE
const char* mrb_debug_next(void)
{
#ifdef MRB_USE_DEBUG_HOOK
  if (!global_mrb || g_dbg.mode != WASM_DBG_PAUSED) {
    return debug_json_error("not paused");
  }

  /* Record current position and call depth */
  g_dbg.prev_file = g_dbg.pause_file;
  g_dbg.prev_line = g_dbg.pause_line;
  g_dbg.prev_ci = global_mrb->c->ci;

  /* Clean up current binding */
  if (!mrb_nil_p(g_dbg.binding)) {
    mrb_gc_unregister(global_mrb, g_dbg.binding);
    g_dbg.binding = mrb_nil_value();
  }

  /* Set next mode and install hook */
  g_dbg.mode = WASM_DBG_NEXT;
  global_mrb->code_fetch_hook = wasm_code_fetch_hook;

  /* Resume the suspended task (unregister before resume; see continue) */
  mrb_value task = g_dbg.paused_task;
  g_dbg.paused_task = mrb_nil_value();
  if (!mrb_nil_p(task)) {
    mrb_gc_unregister(global_mrb, task);
    mrb_resume_task(global_mrb, task);
  }

  if (g_dbg.mode == WASM_DBG_PAUSED) {
    return mrb_debug_get_status();
  }
  return "{\"status\":\"stepping\"}";
#else
  return debug_json_error("debug hook not available");
#endif
}

// Get call stack of the paused task
EMSCRIPTEN_KEEPALIVE
const char* mrb_debug_get_callstack(void)
{
  if (!global_mrb || g_dbg.mode != WASM_DBG_PAUSED) {
    return debug_json_error("not paused");
  }
  if (mrb_nil_p(g_dbg.paused_task)) {
    return debug_json_error("no paused task");
  }

  /* Get the task's context.
     NULL type arg: mrb_task_type is static in task.c,
     but g_dbg.paused_task is always set by our own code. */
  mrb_task *t = (mrb_task *)mrb_data_check_get_ptr(
    global_mrb, g_dbg.paused_task, NULL);
  if (!t) {
    return debug_json_error("invalid task");
  }

  struct mrb_context *c = &t->c;
  if (!c->cibase || !c->ci) {
    return debug_json_error("no call stack");
  }

  /* Build JSON array of stack frames */
  char *p = json_buffer;
  size_t remaining = JSON_BUFFER_SIZE - 1;
  *p++ = '[';
  remaining--;

  mrb_bool first = TRUE;
  mrb_callinfo *ci;

  for (ci = c->ci; ci >= c->cibase; ci--) {
    const struct RProc *proc = ci->proc;
    if (!proc || MRB_PROC_CFUNC_P(proc)) continue;

    const mrb_irep *irep = proc->body.irep;
    uint32_t pc_val = 0;
    if (ci->pc && irep->iseq) {
      pc_val = (uint32_t)(ci->pc - irep->iseq);
      if (pc_val > 0) pc_val--;
    }

    const char *file = NULL;
    int32_t line = 0;
    mrb_debug_get_position(global_mrb, irep, pc_val, &line, &file);

    /* Get method name from ci->mid */
    const char *method_name = "<main>";
    if (ci->mid != 0) {
      method_name = mrb_sym_name(global_mrb, ci->mid);
      if (!method_name) method_name = "<unknown>";
    }

    if (!first && remaining > 2) {
      *p++ = ',';
      remaining--;
    }
    first = FALSE;

    /* Build frame JSON object */
    int n = snprintf(p, remaining, "{\"method\":");
    p += n; remaining -= (size_t)n;
    append_json_string(&p, &remaining, method_name);

    n = snprintf(p, remaining, ",\"file\":");
    p += n; remaining -= (size_t)n;
    append_json_string(&p, &remaining, file ? file : "(unknown)");

    n = snprintf(p, remaining, ",\"line\":%d}", line);
    p += n; remaining -= (size_t)n;

    remaining = JSON_BUFFER_SIZE - (p - json_buffer) - 1;
    if (remaining < 128) break;
  }

  if (remaining > 2) {
    *p++ = ']';
    *p = '\0';
  }

  return json_buffer;
}
