#include <emscripten.h>

#include "mruby.h"
#include "mruby/string.h"

#include "mruby_compiler.h"
#include "mrc_utils.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define picorb_utf8_from_locale(p, l) ((char*)(p))
#define picorb_utf8_free(p)

//extern void mrb_init_picoruby_gems(mrb_state *mrb);
extern void mrb_js_init(mrb_state *mrb);
extern void mrb_websocket_init(mrb_state *mrb);
#ifdef PICORUBY_DEBUG
extern void mrb_wasm_debugger_init(mrb_state *mrb);
#endif

mrb_state *global_mrb = NULL;
mrb_value main_task = {0};

void*
FILE_physical_address(void* p)
{
  return p;
}

size_t
FILE_sector_size(void* p)
{
  return 1;
}

EMSCRIPTEN_KEEPALIVE
void
mrb_tick_wasm(void)
{
  if (!global_mrb) return;
  mrb_tick(global_mrb);
}

EMSCRIPTEN_KEEPALIVE
int
mrb_run_step(void)
{
  if (!global_mrb) return -1;

  mrb_value result = mrb_task_run_once(global_mrb);
  (void)result;
  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      fprintf(stderr, "Exception in main loop (failed to inspect exception)\n");
      global_mrb->exc = NULL;
    } else {
      fprintf(stderr, "Exception in main loop: %s\n", RSTRING_PTR(exc_str));
    }
    return -1;
  }

  // Even if there is no task to run, return 0
  // so to wait for callbacks like event listener
  return 0;
}

EMSCRIPTEN_KEEPALIVE
int
picorb_init(void)
{
  global_mrb = mrb_open();
  if (!global_mrb) {
    fprintf(stderr, "Failed to initialize mruby state\n");
    return -1;
  }

 // mrb_init_picoruby_gems(global_mrb);
  mrb_js_init(global_mrb);
  mrb_websocket_init(global_mrb);
#ifdef PICORUBY_DEBUG
  mrb_wasm_debugger_init(global_mrb);
#endif

  const uint8_t *script = (const uint8_t *)"Task.current.suspend";
  size_t size = strlen((const char *)script);
  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  if (!cc) {
    fprintf(stderr, "Failed to create compiler context\n");
    return -1;
  }
  mrc_irep *irep = mrc_load_string_cxt(cc, &script, size);
  if (!irep) {
    fprintf(stderr, "Failed to compile main task script\n");
    mrc_ccontext_free(cc);
    return -1;
  }

  main_task = mrc_create_task(cc, irep, mrb_str_new_lit(global_mrb, "main"),
                               mrb_nil_value(), mrb_obj_value(global_mrb->object_class));
  mrc_ccontext_free(cc);

  return 0;
}

EMSCRIPTEN_KEEPALIVE
int
picorb_create_task(const char *code)
{
  if (!global_mrb) {
    fprintf(stderr, "mruby state not initialized\n");
    return -1;
  }

  char* utf8 = picorb_utf8_from_locale(code, -1);
  if (!utf8) {
    fprintf(stderr, "Failed to convert code to UTF-8\n");
    return -1;
  }

  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  if (!cc) {
    fprintf(stderr, "Failed to create mruby compiler context\n");
    picorb_utf8_free(utf8);
    return -1;
  }

  const uint8_t *script_ptr = (const uint8_t *)utf8;
  size_t size = strlen(utf8);
  mrc_irep *irep = mrc_load_string_cxt(cc, &script_ptr, size);

  picorb_utf8_free(utf8);

  if (!irep) {
    fprintf(stderr, "Failed to compile script\n");
    mrc_ccontext_free(cc);
    return -1;
  }

  mrb_value task = mrc_create_task(cc, irep, mrb_nil_value(), mrb_nil_value(), mrb_obj_value(global_mrb->object_class));
  mrc_ccontext_free(cc);

  if (mrb_nil_p(task)) {
    fprintf(stderr, "Failed to create task\n");
    return -1;
  }

  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      fprintf(stderr, "Ruby exception (failed to inspect exception)\n");
      global_mrb->exc = NULL;
    } else {
      fprintf(stderr, "Ruby exception: %s\n", RSTRING_PTR(exc_str));
    }
    return -1;
  }

  return 0;
}

// Forward declaration to avoid including mruby/dump.h and causing redefinition errors.
struct mrb_irep;
struct mrb_irep* mrb_read_irep_buf(mrb_state *mrb, const void *buf, size_t bufsize);

EMSCRIPTEN_KEEPALIVE
int
picorb_create_task_from_mrb(const char *mrb_data, size_t data_len)
{
  if (!global_mrb) {
    fprintf(stderr, "mruby state not initialized\n");
    return -1;
  }

  // mrb_read_irep_buf returns `mrb_irep*`, but `mrc_create_task` expects `mrc_irep*`.
  // Other parts of the codebase suggest a direct cast is acceptable as the structs are compatible.
  mrc_irep *irep = (mrc_irep *)mrb_read_irep_buf(global_mrb, (const uint8_t *)mrb_data, data_len);

  if (!irep) {
    fprintf(stderr, "Failed to load mrb data\n");
    return -1;
  }

  mrc_ccontext *cc = mrc_ccontext_new(global_mrb);
  if (!cc) {
    fprintf(stderr, "Failed to create mruby compiler context\n");
    return -1;
  }

  mrb_value task = mrc_create_task(cc, irep, mrb_nil_value(), mrb_nil_value(), mrb_obj_value(global_mrb->object_class));
  mrc_ccontext_free(cc);

  if (mrb_nil_p(task)) {
    fprintf(stderr, "Failed to create task from mrb\n");
    return -1;
  }

  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    global_mrb->exc = NULL;
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    if (global_mrb->exc) {
      fprintf(stderr, "Ruby exception in mrb task (failed to inspect exception)\n");
      global_mrb->exc = NULL;
    } else {
      fprintf(stderr, "Ruby exception in mrb task: %s\n", RSTRING_PTR(exc_str));
    }
    return -1;
  }

  return 0;
}

void
mrb_picoruby_wasm_gem_init(mrb_state* mrb)
{
  mrb_js_init(mrb);
  mrb_websocket_init(mrb);
}

void
mrb_picoruby_wasm_gem_final(mrb_state* mrb)
{
}
