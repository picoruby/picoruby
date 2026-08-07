#ifndef HAL_PORTING_H_
#define HAL_PORTING_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(PICORB_VM_MRUBYC)
  #define picorb_hal_init         mrbc_hal_init
  #define picorb_hal_final        mrbc_hal_final
  #define picorb_hal_enable_irq   mrbc_hal_enable_irq
  #define picorb_hal_disable_irq  mrbc_hal_disable_irq
  #define picorb_hal_idle_cpu     mrbc_hal_idle_cpu
  #define picorb_hal_sleep_us     mrbc_hal_sleep_us
  #define picorb_tick             mrbc_tick
  #define picorb_hal_write        mrbc_hal_write
  #define picorb_hal_abort        mrbc_hal_abort
  #define picorb_hal_flush        mrbc_hal_flush
#else
  #define picorb_hal_init         mrb_hal_task_init
  #define picorb_hal_final        mrb_hal_task_final
  #define picorb_hal_enable_irq   mrb_task_enable_irq
  #define picorb_hal_disable_irq  mrb_task_disable_irq
  #define picorb_hal_idle_cpu     mrb_hal_task_idle_cpu
  #define picorb_hal_sleep_us     mrb_hal_task_sleep_us
  #define picorb_tick             mrb_tick
  #define picorb_hal_write        mrb_hal_write
  #define picorb_hal_abort        mrb_hal_abort
  #define picorb_hal_flush        mrb_hal_flush
#endif

#if defined(PICORB_VM_MRUBY)
#include "mruby.h"
void picorb_tick(mrb_state *mrb);
void picorb_hal_init(mrb_state *mrb);
void picorb_hal_idle_cpu(mrb_state *mrb);

#if defined(MRB_USE_TASK_SCHEDULER)
/*
 * Scheduler services
 *
 * mruby's scheduler hook is a single slot per mrb_state and setting it
 * replaces whatever was there -- upstream leaves composition to the
 * embedder on purpose. PicoRuby has more than one thing to service at a
 * scheduler entry (the cyw43_arch poll pump, the IRQ event bridge), so
 * the slot belongs to the dispatcher below and gems register here
 * instead of calling mrb_task_set_scheduler_hook themselves.
 *
 * Same rules as the hook itself: runs in thread context before the
 * ready-queue read, must be cheap, must never sleep or re-enter the
 * scheduler. Registration order carries no meaning -- services must be
 * independent. Registering the same fn/ud twice is a no-op.
 *
 * The table belongs to one mrb_state at a time, matching the slot it
 * feeds. Registering from a different VM discards the previous VM's
 * entries, so a service that outlives an mrb_close (one registered from
 * a HAL init, say) is re-registered rather than duplicated.
 */
#define PICORB_SCHEDULER_SERVICE_MAX 4
void picorb_scheduler_service_add(mrb_state *mrb, void (*fn)(mrb_state *mrb, void *ud), void *ud);
void picorb_scheduler_service_remove(mrb_state *mrb, void (*fn)(mrb_state *mrb, void *ud), void *ud);
#endif

#elif defined(PICORB_VM_MRUBYC)
void picorb_tick();
void picorb_hal_init(void);
void picorb_hal_idle_cpu(void);

#if defined(MRBC_TASK_SCHEDULER_HOOK)
/* Same contract as the mruby side above. mruby/c's hook is process-
 * global rather than per-VM, so there is no owner to track and the
 * callback takes no VM argument. */
#define PICORB_SCHEDULER_SERVICE_MAX 4
void picorb_scheduler_service_add(void (*fn)(void *ud), void *ud);
void picorb_scheduler_service_remove(void (*fn)(void *ud), void *ud);
#endif

#ifndef picorb_SCHEDULER_EXIT
#define picorb_SCHEDULER_EXIT 1
#endif
#endif

int picorb_hal_write(int fd, const void *buf, int nbytes);
bool picorb_hal_cdc_connected(uint8_t itf);
int picorb_hal_cdc_write(uint8_t itf, const void *buf, int nbytes, uint32_t timeout_ms);
void picorb_hal_enable_irq(void);
void picorb_hal_disable_irq(void);
void picorb_hal_abort(const char *s);
int picorb_hal_flush(int fd);

#define HAL_GETCHAR_NODATA  (-1)
#define HAL_GETCHAR_EOF     (-2)

int picorb_hal_read_available(void);
int picorb_hal_getchar(void);
/* Push a byte into the stdin ring buffer.
 * Returns true on success, false if the buffer is full (byte NOT stored). */
bool picorb_hal_stdin_push(uint8_t ch);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // HAL_PORTING_H_
