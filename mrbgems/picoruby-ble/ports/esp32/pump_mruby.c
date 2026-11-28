#include "nimble_owner.h"
#include "../../../picoruby-machine/include/hal.h"
#include "mruby/error.h"

static mrb_value
pump_body(mrb_state *mrb, void *ud)
{
  (void)mrb;
  (void)ud;
  picoruby_nimble_pump();
  return mrb_nil_value();
}

static void
pump_service(mrb_state *mrb, void *ud)
{
  (void)ud;
  mrb_bool error = FALSE;
  int ai = mrb_gc_arena_save(mrb);
  mrb_protect_error(mrb, pump_body, NULL, &error);
  mrb_gc_arena_restore(mrb, ai);
}

void
picoruby_nimble_attach_vm(void *vm)
{
  if (vm == NULL) return;
  picorb_scheduler_service_add((mrb_state *)vm, pump_service, NULL);
}
