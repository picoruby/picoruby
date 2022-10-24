#ifndef HAL_NO_IMPL_H_
#define HAL_NO_IMPL_H_

#ifdef __cplusplus
extern "C" {
#endif

void mrbc_tick(void);
void hal_init(void);
void hal_enable_irq(void);
void hal_disable_irq(void);
void hal_abort(const char *s);
int hal_write(int fd, const void *buf, int nbytes);
int hal_flush(int fd);

#ifdef __cplusplus
}
#endif

#endif // ifndef HAL_NO_IMPL_H_
