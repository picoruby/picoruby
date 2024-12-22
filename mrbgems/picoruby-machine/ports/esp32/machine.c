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

#include "driver/uart.h"

static volatile uint32_t interrupt_nesting = 0;

void
hal_init(void)
{
}

void
hal_enable_irq()
{
  portENABLE_INTERRUPTS();
}

void
hal_disable_irq()
{
  portDISABLE_INTERRUPTS();
}

void
hal_idle_cpu()
{
  vTaskDelay(1);
}

int
hal_write(int fd, const void *buf, int nbytes)
{
  return printf(buf);
}

int hal_flush(int fd) {
  return fflush(stdout);
}

int
hal_read_available(void)
{
  hal_idle_cpu();
  return 1;
}

int
hal_getchar(void)
{
  if (hal_read_available()) {
    return getchar();
  }
  return -1;
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
}

bool
Machine_tud_mounted_q(void)
{
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
}

void
Machine_delay_ms(uint32_t ms)
{
  vTaskDelay(ms);
}

void
Machine_busy_wait_ms(uint32_t ms)
{
}

int
Machine_get_unique_id(char *id_str)
{
  return 0;
}
