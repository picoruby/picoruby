#include <emscripten.h>

#include <mrubyc.h>

#include <mrc_common.h>
#include <mrc_ccontext.h>
#include <mrc_compile.h>
#include <mrc_dump.h>
#include <picogem_init.c>

#define picorb_utf8_from_locale(p, l) ((char*)(p))
#define picorb_utf8_free(p)

#if !defined(PICORUBY_MEMORY_SIZE)
#define PICORUBY_MEMORY_SIZE (1024*1000)
#endif
static uint8_t memory_pool[PICORUBY_MEMORY_SIZE];

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
  mrbc_init(memory_pool, PICORUBY_MEMORY_SIZE);
  picoruby_init_require(NULL);
  return 0;
}

EMSCRIPTEN_KEEPALIVE
int
picorb_create_task(const char *code)
{
  char* utf8 = picorb_utf8_from_locale(code, -1);
  if (!utf8) abort();

  mrc_ccontext *c = mrc_ccontext_new(NULL);
  mrc_irep *irep = mrc_load_string_cxt(c, (const uint8_t **)&utf8, strlen(utf8));
  picorb_utf8_free(utf8);

  uint8_t *mrb = NULL;
  size_t mrb_size = 0;

  int result;
  result = mrc_dump_irep(c, irep, 0, &mrb, &mrb_size);
  if (result != MRC_DUMP_OK) {
    fprintf(stderr, "irep dump error: %d\n", result);
    exit(EXIT_FAILURE);
  }

  mrbc_create_task(mrb, NULL);
  return 0;
}

void
mrbc_wasm_init(mrbc_vm *vm)
{
  mrbc_js_init(vm);
}
