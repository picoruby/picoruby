#include "../../include/machine.h"
#include <stdio.h>

// Format: "1d 02:03:04.05"
void
Machine_uptime_formatted(char *buf, int maxlen)
{
  uint64_t us = (uint64_t)Machine_uptime_us();
  uint32_t sec = us / 1000000;
  uint32_t min = sec / 60;
  uint32_t hour  = min / 60;
  uint32_t day  = hour / 24;
  snprintf(
    buf,
    maxlen,
    "%ud %02u:%02u:%02u.%02u",
    (unsigned int)day,
    (unsigned int)hour % 24,
    (unsigned int)min % 60,
    (unsigned int)sec % 60,
    (unsigned int)(us % 1000000) / 10000
  );
}
