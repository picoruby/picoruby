#ifndef MRBC_SRC_HAL_H_
#define MRBC_SRC_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif

void mrbc_tick(void);

/***** Macros ***************************************************************/
#if !defined(MRBC_TICK_UNIT)
#define MRBC_TICK_UNIT_1_MS   1
#define MRBC_TICK_UNIT_2_MS   2
#define MRBC_TICK_UNIT_4_MS   4
#define MRBC_TICK_UNIT_10_MS 10
// You may be able to reduce power consumption if you configure
// MRBC_TICK_UNIT_2_MS or larger.
#define MRBC_TICK_UNIT MRBC_TICK_UNIT_1_MS
// Substantial timeslice value (millisecond) will be
// MRBC_TICK_UNIT * MRBC_TIMESLICE_TICK_COUNT (+ Jitter).
// MRBC_TIMESLICE_TICK_COUNT must be natural number
// (recommended value is from 1 to 10).
#define MRBC_TIMESLICE_TICK_COUNT 10

#endif
#if !defined(MRBC_NO_TIMER)
void hal_init(void);
void hal_enable_irq(void);
void hal_disable_irq(void);
void __wfi(void);
# define hal_idle_cpu()   __wfi()

#else // MRBC_NO_TIMER
#define hal_init()        ((void)0)
#define hal_enable_irq()  ((void)0)
#define hal_disable_irq() ((void)0)
#define hal_idle_cpu()    (sleep_ms(1), mrbc_tick())
#endif

void hal_abort(const char *s);

int hal_write(int fd, const void *buf, int nbytes);
int hal_flush(int fd);

#ifdef __cplusplus
}
#endif
#endif // ifndef MRBC_HAL_H_
