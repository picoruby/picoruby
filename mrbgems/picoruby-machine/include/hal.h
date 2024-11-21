#ifndef HAL_PORTING_H_
#define HAL_PORTING_H_

#ifdef __cplusplus
extern "C" {

#endif
#ifndef MRBC_NO_TIMER
void hal_init(void);
void hal_enable_irq(void);
void hal_disable_irq(void);
#else // MRBC_NO_TIMER
# define hal_init()        ((void)0)
# define hal_enable_irq()  ((void)0)
# define hal_disable_irq() ((void)0)
#endif
void hal_idle_cpu(void);
void hal_abort(const char *s);
int hal_write(int fd, const void *buf, int nbytes);
int hal_flush(int fd);
int hal_read_available(void);
int hal_getchar(void);

void mrbc_tick(void);

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // HAL_PORTING_H_
