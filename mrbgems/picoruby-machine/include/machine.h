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

/*
 * Low-power sleep. Machine.sleep(deep:, source:, ...) in mrblib maps
 * onto these two entry points; the mode x wake-source matrix is
 * documented in README.md. Wake always continues execution.
 *
 * The result tells the glue which exception to raise. A single
 * ArgumentError cannot represent what actually happens down here:
 * the SDK failing to claim a hardware alarm is not a caller mistake.
 */
typedef enum {
  MACHINE_SLEEP_OK = 0,
  MACHINE_SLEEP_EINVAL,        /* bad argument             -> ArgumentError */
  MACHINE_SLEEP_EUNSUPPORTED,  /* no support on this port  -> NotImplementedError */
  MACHINE_SLEEP_ERESOURCE,     /* SDK could not claim h/w  -> RuntimeError */
  MACHINE_SLEEP_ESTATE,        /* machine state forbids it -> RuntimeError */
} machine_sleep_result_t;

/* pin is int, not uint8_t: a uint8_t would wrap 256 to GPIO 0 and
 * sleep on the wrong pin. The glue rejects values outside
 * [0, INT32_MAX]; the port checks the platform range. */
machine_sleep_result_t Machine_sleep_timer(bool deep, uint32_t ms);
machine_sleep_result_t Machine_sleep_gpio(bool deep, int pin, bool edge, bool high);
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
bool Machine_bootsel_pressed_q(void);

#ifdef __cplusplus
}
#endif

#endif /* MACHINE_DEFINED_H_ */
