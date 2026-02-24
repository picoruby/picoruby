#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#ifdef __APPLE__
#include <unistd.h>
#include <uuid/uuid.h>
#endif

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
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, NULL);
}

void
Machine_busy_wait_us(uint32_t us)
{
  struct timespec ts;
  ts.tv_sec = us / 1000000;
  ts.tv_nsec = (us % 1000000) * 1000;
  nanosleep(&ts, NULL);
}

void
Machine_sleep(uint32_t seconds)
{
}

bool
Machine_get_unique_id(char *id_str)
{
#ifdef __APPLE__
  uuid_t uuid;
  struct timespec timeout = {0, 0};
  if (gethostuuid(uuid, &timeout) == 0) {
    /* Convert 16-byte UUID to 32-character hex string */
    for (int i = 0; i < 16; i++) {
      sprintf(&id_str[i * 2], "%02x", uuid[i]);
    }
    id_str[32] = '\0';
    return true;
  }
  perror("Failed to get host UUID");
  return false;
#else
  FILE *fp = fopen("/etc/machine-id", "r");
  if (fp) {
    if (fgets(id_str, 33, fp) == NULL) {
      perror("Failed to read /etc/machine-id");
      fclose(fp);
      return false;
    }
    fclose(fp);
    return true;
  }
  perror("Failed to open /etc/machine-id");
  return false;
#endif
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
  exit(status);
}

uint64_t
Machine_uptime_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}
