#include <emscripten.h>

#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/presym.h"
#include "mruby/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define picorb_utf8_from_locale(p, l) ((char*)(p))
#define picorb_utf8_free(p)

extern void mrb_init_picoruby_gems(mrb_state *mrb);
extern void mrb_js_init(mrb_state *mrb);

mrb_state *global_mrb = NULL;

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
int
picorb_init(void)
{
  global_mrb = mrb_open();
  if (!global_mrb) {
    fprintf(stderr, "Failed to initialize mruby state\n");
    return -1;
  }

  mrb_init_picoruby_gems(global_mrb);
  mrb_js_init(global_mrb);

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

  mrbc_context *c = mrbc_context_new(global_mrb);
  if (!c) {
    fprintf(stderr, "Failed to create mruby compiler context\n");
    picorb_utf8_free(utf8);
    return -1;
  }

  mrb_load_string_cxt(global_mrb, utf8, c);
  picorb_utf8_free(utf8);
  mrbc_context_free(global_mrb, c);

  if (global_mrb->exc) {
    mrb_value exc = mrb_obj_value(global_mrb->exc);
    mrb_value exc_str = mrb_inspect(global_mrb, exc);
    fprintf(stderr, "Ruby exception: %s\n", RSTRING_PTR(exc_str));
    global_mrb->exc = NULL;
    return -1;
  }

  return 0;
}

void
mrb_picoruby_wasm_gem_init(mrb_state* mrb)
{
  mrb_js_init(mrb);
}

void
mrb_picoruby_wasm_gem_final(mrb_state* mrb)
{
}
