#include "../../include/hal.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/task.h>

#include "esp_sleep.h"
#include "esp_timer.h"
#include "hal/efuse_hal.h"

#define ESP32_MSEC_PER_TICK       (10)
#define ESP32_TIMER_UNIT_PER_SEC  (1000000)

#ifdef MRBC_NO_TIMER
  #error "MRBC_NO_TIMER is not supported"
#endif

#if defined(PICORB_VM_MRUBY)
static mrb_state *mrb_;
#endif

static esp_timer_handle_t periodic_timer;

static void
alarm_handler(void *arg)
{
#if defined(PICORB_VM_MRUBYC)
  mrbc_tick();
#else
  mrb_tick(mrb_);
#endif
}

void
#if defined(PICORB_VM_MRUBY)
machine_hal_init(mrb_state *mrb)
#elif defined(PICORB_VM_MRUBYC)
machine_hal_init(void)
#endif
{
#if defined(PICORB_VM_MRUBY)
  mrb_ = (mrb_state *)mrb;
#endif
  esp_timer_create_args_t timer_create_args;
  timer_create_args.callback = &alarm_handler;
  timer_create_args.arg = NULL;
  timer_create_args.dispatch_method = ESP_TIMER_TASK;
  timer_create_args.name = "mrbc_tick_timer";

  esp_timer_create(&timer_create_args, &periodic_timer);
#if defined(PICORB_VM_MRUBY)
  esp_timer_start_periodic(periodic_timer, MRB_TICK_UNIT * 1000);
#else
  esp_timer_start_periodic(periodic_timer, MRBC_TICK_UNIT * 1000);
#endif
}

void
#if defined(PICORB_VM_MRUBYC)
hal_enable_irq(void)
#elif defined(PICORB_VM_MRUBY)
mrb_task_enable_irq()
#endif
{
  portENABLE_INTERRUPTS();
}

void
#if defined(PICORB_VM_MRUBYC)
hal_disable_irq(void)
#elif defined(PICORB_VM_MRUBY)
mrb_task_disable_irq()
#endif
{
  portDISABLE_INTERRUPTS();
}

void
#if defined(PICORB_VM_MRUBYC)
hal_idle_cpu()
#elif defined(PICORB_VM_MRUBY)
hal_idle_cpu(mrb_state *mrb)
#endif
{
  vTaskDelay(1);
}

int
hal_write(int fd, const void *buf, int nbytes)
{
  FILE *stream = (fd == 1) ? stdout : stderr;
  for (int i = 0 ; i < nbytes ; i++) {
    fputc(((char*)buf)[i], stream);
  }
  fflush(stream);
  return nbytes;
}

int hal_flush(int fd) {
  FILE *stream = (fd == 1) ? stdout : stderr;
  return fflush(stream);
}

int
hal_read_available(void)
{
  int c = fgetc(stdin);
  return ungetc(c, stdin) == EOF ? 0 : 1;
}

int
hal_getchar(void)
{
  return fgetc(stdin);
}

void
hal_abort(const char *s)
{
  if(s) {
    hal_write(1, s, strlen(s));
  }

  abort();
}


/*-------------------------------------
 *
 * USB
 *
 *------------------------------------*/

void
Machine_tud_task(void)
{
  /* Not required for ESP32 */
}

bool
Machine_tud_mounted_q(void)
{
  /* Not required for ESP32 */
  return 0;
}


/*-------------------------------------
 *
 * RTC
 *
 *------------------------------------*/

/*
 * deep_sleep doesn't work yet
 */
void
Machine_deep_sleep(uint8_t gpio_pin, bool edge, bool high)
{
}

void
Machine_sleep(uint32_t seconds)
{
  esp_sleep_enable_timer_wakeup(seconds * ESP32_TIMER_UNIT_PER_SEC);
  esp_light_sleep_start();
}

void
Machine_delay_ms(uint32_t ms)
{
  vTaskDelay(ms / ESP32_MSEC_PER_TICK);
}

/*
 * busy_wait_ms doesn't work yet
 */
void
Machine_busy_wait_ms(uint32_t ms)
{
}

bool
Machine_get_unique_id(char *id_str)
{
  uint8_t mac[6];
  efuse_hal_get_mac(mac);
  sprintf(id_str, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return true;
}

uint32_t
Machine_stack_usage(void)
{
  // Not implemented
  return 0;
}

const char *
Machine_mcu_name(void)
{
  return "ESP32";
}

bool
Machine_set_hwclock(const struct timespec *ts)
{
  // Not implemented
  return false;
}

bool
Machine_get_hwclock(struct timespec *ts)
{
  // Not implemented
  return false;
}

void
Machine_exit(int status)
{
  (void)status; // no-op
}

uint64_t
Machine_uptime_us(void)
{
  return (uint64_t)esp_timer_get_time();
}
