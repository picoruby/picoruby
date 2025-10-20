#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>

#include "../../include/machine.h"


void
Machine_tud_task(void)
{
}

bool
Machine_tud_mounted_q(void)
{
  return true;
}

void
Machine_delay_ms(uint32_t ms)
{
}

void
Machine_busy_wait_ms(uint32_t ms)
{
}

void
Machine_sleep(uint32_t seconds)
{
}

bool
Machine_get_unique_id(char *id_str)
{
  FILE *fp = fopen("/etc/machine-id", "r");
  if (fp) {
    if (fgets(id_str, 32, fp) == NULL) {
      perror("Failed to read /etc/machine-id");
      fclose(fp);
      return false;
    }
    fclose(fp);
    return true;
  }
  perror("Failed to open /etc/machine-id");
  return false;
}

uint32_t
Machine_stack_usage(void)
{
  return 0;
}

const char *
Machine_mcu_name(void)
{
  return "POSIX";
}

void
Machine_exit(int status)
{
  sigint_status = MACHINE_SIGINT_EXIT;
  exit_status = status;
  raise(SIGINT);
}

uint64_t
Machine_uptime_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}
