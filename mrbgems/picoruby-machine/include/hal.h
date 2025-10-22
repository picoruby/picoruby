#ifndef HAL_PORTING_H_
#define HAL_PORTING_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(PICORB_VM_MRUBY)
#include "mruby.h"
void mrb_tick(mrb_state *mrb);
void hal_init(mrb_state *mrb);
#elif defined(PICORB_VM_MRUBYC)
void mrbc_tick();
void hal_init(void);
#endif

int hal_write(int fd, const void *buf, int nbytes);

#if defined(PICORB_VM_MRUBYC)
  void hal_enable_irq(void);
  void hal_disable_irq(void);
#else
  void mrb_task_enable_irq(void);
  void mrb_task_disable_irq(void);
#endif

void hal_abort(const char *s);
int hal_flush(int fd);
int hal_read_available(void);
int hal_getchar(void);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // HAL_PORTING_H_
