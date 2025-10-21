#include "../../include/machine.h"
#include <stdio.h>

// Format: "1days 02:03:04"
void
Machine_uptime_formatted(char *buf, int maxlen)
{
  uint64_t us = (uint64_t)Machine_uptime_us();
  uint32_t sec = us / 1000000;
  uint32_t min = sec / 60;
  uint32_t hour  = min / 60;
  uint32_t day  = hour / 24;
  snprintf(buf, maxlen, "%udays %02u:%02u:%02u", day, hour % 24, min % 60, sec % 60);
}
