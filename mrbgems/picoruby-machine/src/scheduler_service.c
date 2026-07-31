/*
 * Multiplexer for mruby's single scheduler-hook slot.
 *
 * See the contract in include/hal.h. The table is plain static state
 * written only at gem init/final time and read only from the VM thread,
 * so it needs no locking.
 */

#include "../include/hal.h"

#if defined(PICORB_VM_MRUBY) && defined(MRB_USE_TASK_SCHEDULER)

#include <mruby.h>
#include "task.h"

typedef struct {
  void (*fn)(mrb_state *mrb, void *ud);
  void *ud;
} scheduler_service_t;

static scheduler_service_t services_[PICORB_SCHEDULER_SERVICE_MAX];
static int service_count_;
/* The table is process-global but the hook slot it feeds belongs to one
   mrb_state, so the table has to belong to that same VM. PicoRuby runs
   one VM at a time; when a different one shows up the previous one is
   gone, and whatever it registered goes with it. */
static mrb_state *owner_;

static void
scheduler_dispatch(mrb_state *mrb, void *ud)
{
  int i = 0;

  (void)ud;
  while (i < service_count_) {
    services_[i].fn(mrb, services_[i].ud);
    i++;
  }
}

void
picorb_scheduler_service_add(mrb_state *mrb, void (*fn)(mrb_state *mrb, void *ud), void *ud)
{
  int i = 0;

  if (fn == NULL) return;
  if (owner_ != mrb) {
    /* A new VM: drop entries left over from the previous one rather
       than call into whatever they closed over. */
    service_count_ = 0;
    owner_ = mrb;
  }
  while (i < service_count_) {
    if (services_[i].fn == fn && services_[i].ud == ud) return;
    i++;
  }
  if (PICORB_SCHEDULER_SERVICE_MAX <= service_count_) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "too many scheduler services");
  }
  services_[service_count_].fn = fn;
  services_[service_count_].ud = ud;
  service_count_++;
  mrb_task_set_scheduler_hook(mrb, scheduler_dispatch, NULL);
}

void
picorb_scheduler_service_remove(mrb_state *mrb, void (*fn)(mrb_state *mrb, void *ud), void *ud)
{
  int i = 0;

  if (owner_ != mrb) return;
  while (i < service_count_) {
    if (services_[i].fn == fn && services_[i].ud == ud) {
      /* Services are independent, so filling the hole with the last
         entry is fine and keeps removal O(1). */
      service_count_--;
      services_[i] = services_[service_count_];
      break;
    }
    i++;
  }
  if (service_count_ == 0) {
    mrb_task_set_scheduler_hook(mrb, NULL, NULL);
    owner_ = NULL;
  }
}

#endif /* PICORB_VM_MRUBY && MRB_USE_TASK_SCHEDULER */
