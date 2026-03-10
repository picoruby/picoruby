#ifndef MACHINE_DEFINED_H_
#define MACHINE_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#if defined(PICORB_PLATFORM_POSIX)
#include <signal.h>
#endif

/* File descriptor to CDC instance mapping for dual CDC configuration */
#define FD_STDOUT 1
#define FD_STDERR 2
#define CDC_INSTANCE_STDOUT 0
#define CDC_INSTANCE_STDERR 1

#ifdef __cplusplus
extern "C" {
#endif

enum {
  MACHINE_SIG_NONE = 0,
  MACHINE_SIGINT_EXIT,
  MACHINE_SIGINT_RECEIVED,
  MACHINE_SIGTSTP_RECEIVED,
};

#if defined(PICORB_PLATFORM_POSIX)
__attribute__((weak)) volatile sig_atomic_t sigint_status;
__attribute__((weak)) int exit_status;
#else
extern volatile int sigint_status;
#endif

void Machine_sleep(uint32_t seconds);
void Machine_deep_sleep(uint8_t gpio_pin, bool edge, bool high);
void Machine_delay_ms(uint32_t ms);
void Machine_busy_wait_ms(uint32_t ms);
void Machine_busy_wait_us(uint32_t us);
bool Machine_get_unique_id(char *id_str);
void Machine_tud_task(void);
bool Machine_tud_mounted_q(void);
uint32_t Machine_stack_usage(void);
bool Machine_set_hwclock(const struct timespec *ts);
bool Machine_get_hwclock(struct timespec *ts);
void Machine_exit(int status);
void Machine_reboot(void);
uint64_t Machine_uptime_us(void);

#define MACHINE_EXIT_REBOOT 120
void Machine_uptime_formatted(char *buf, int maxlen);

#ifdef __cplusplus
}
#endif

#endif /* MACHINE_DEFINED_H_ */
