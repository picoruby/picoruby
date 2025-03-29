#ifndef HAL_PORTING_H_
#define HAL_PORTING_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(PICORB_VM_MRUBY)
#include "mruby.h"
void mrb_tick(mrb_state *mrb);
void hal_init(mrb_state *mrb);
int hal_write(int fd, const void *buf, int nbytes);
//#elif defined(PICORB_VM_MRUBYC)
//#include "mrubyc.h"
//typedef void mrb_state;
//void mrbc_tick();
//void hal_init(mrb_state *mrb);
//#define mrb_tick(mrb) mrbc_tick()
//#define MRB_TICK_UNIT MRBC_TICK_UNIT
#endif

void hal_enable_irq(void);
void hal_disable_irq(void);

void hal_idle_cpu(void);
void hal_abort(const char *s);
int hal_flush(int fd);
int hal_read_available(void);
int hal_getchar(void);


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // HAL_PORTING_H_
