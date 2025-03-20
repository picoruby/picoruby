#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>


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
    fgets(id_str, 32, fp);
    fclose(fp);
  } else {
    perror("Failed to open /etc/machine-id");
  }
  return true;
}

uint32_t
Machine_stack_usage(void)
{
  return 0;
}
