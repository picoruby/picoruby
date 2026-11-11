#include <mruby_compiler.h>

#if defined(PICORB_VM_MRUBY)
#include "task.h"
#endif

typedef struct sandbox_state {
  mrc_ccontext *cc;
  mrc_irep *irep;
#if defined(PICORB_VM_MRUBY)
  mrb_value task;
#elif defined(PICORB_VM_MRUBYC)
  mrbc_tcb *tcb;
#endif
  uint8_t *vm_code;
#if defined(PICORB_VM_MRUBYC)
  uint8_t *loaded_code; /* compiled here and loaded into the task's VM */
  bool code_owned;      /* vm_code came from the compiler, not the caller */
#endif
  pm_options_t *options;
} SandboxState;


static void
free_ccontext(SandboxState *ss)
{
  if (ss->cc) {
    mrc_ccontext_free(ss->cc);
    ss->cc = NULL;
  }
}

static void
init_options(pm_options_t *options)
{
  if (!options) {
    return;
  }
  pm_string_init(&options->encoding, (const uint8_t *)"UTF-8", 5);
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/sandbox.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/sandbox.c"

#endif
