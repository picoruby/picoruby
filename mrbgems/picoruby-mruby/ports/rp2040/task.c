#include "task_hal.h"

#if defined(PICO_CYW43_ARCH_POLL)
#include "pico/cyw43_arch.h"
#endif

void
mrb_hal_task_switch_hook(mrb_state *mrb, mrb_task_switch_reason reason)
{
  (void)mrb;
  (void)reason;
#if defined(PICO_CYW43_ARCH_POLL)
  /* cyw43_arch POLL mode: cyw43_driver, lwIP and btstack share one
     async_context that is only serviced from thread context. Pump it at every
     scheduler control point (task switch and GC-drain step alike, hence reason
     is ignored) so a compute-bound task or a long GC drain cannot starve the
     network/BLE stack. Guarded by cyw43_is_initialized() so this is a no-op
     until Wi-Fi/BLE has been brought up (e.g. before CYW43.init). Must stay
     cheap and must never sleep -- that is mrb_hal_task_idle_cpu()'s job. */
  if (cyw43_is_initialized(&cyw43_state)) {
    cyw43_arch_poll();
  }
#else
  /* Nothing to service on this platform */
#endif
}
