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
#elif defined(PICORB_VM_MRUBYC)
void picorb_tick();
void picorb_hal_init(void);
void picorb_hal_idle_cpu(void);
#ifndef picorb_SCHEDULER_EXIT
#define picorb_SCHEDULER_EXIT 1
#endif
#endif

int picorb_hal_write(int fd, const void *buf, int nbytes);
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
