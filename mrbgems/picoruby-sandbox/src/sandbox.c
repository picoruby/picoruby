#include <mruby_compiler.h>

#if defined(PICORB_VM_MRUBY)
#include "task.h"
#endif

typedef struct sandbox_state {
  mrc_ccontext *cc;
  mrc_irep *irep;
#if defined(PICORB_VM_MRUBY)
  mrb_tcb *tcb;
#elif defined(PICORB_VM_MRUBYC)
  mrbc_tcb *tcb;
#endif
  uint8_t *vm_code;
  pm_options_t *options;
} SandboxState;


static void
free_ccontext(SandboxState *ss)
{
  if (ss->cc) {
    if (ss->cc->options) {
      pm_options_free(ss->cc->options);
    }
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
  pm_string_t *encoding = &options->encoding;
  pm_string_constant_init(encoding, "UTF-8", 5);
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/sandbox.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/sandbox.c"

#endif
