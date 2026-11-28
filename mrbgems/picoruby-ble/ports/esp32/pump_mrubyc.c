#include <stddef.h>
#include "nimble_owner.h"
#include "../../../picoruby-machine/include/hal.h"

static void
pump_service(void *ud)
{
  (void)ud;
  picoruby_nimble_pump();
}

void
picoruby_nimble_attach_vm(void *vm)
{
  (void)vm;
  picorb_scheduler_service_add(pump_service, NULL);
}
