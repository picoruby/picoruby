#include "task_hal.h"

void
mrb_hal_task_switch_hook(mrb_state *mrb, mrb_task_switch_reason reason)
{
  (void)mrb;
  (void)reason;
  /* Nothing to service on this platform */
}
