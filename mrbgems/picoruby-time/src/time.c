#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../include/picogem_time.h"
#include "env.h"

/* ---- what both VM bindings share ----
 * A Time holds the microseconds since the epoch, the broken-down fields
 * in its own offset, that offset as seconds west of UTC (the sign of the
 * POSIX `timezone` global) and whether it is a UTC time. */

/* seconds since the epoch for the fields of tm taken as UTC: timegm()
 * is not in every libc. Days from civil after Howard Hinnant. */
static int64_t
picorb_timegm(const struct tm *tm)
{
  int64_t y = tm->tm_year + 1900;
  int64_t m = tm->tm_mon + 1;
  int64_t d = tm->tm_mday;
  if (m <= 2) y--;
  int64_t era = (y >= 0 ? y : y - 399) / 400;
  int64_t yoe = y - era * 400;
  int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int64_t days = era * 146097 + doe - 719468;
  return days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

/* Fill tm and *timezone for the unixtime: UTC, a fixed offset in
 * seconds east of UTC, or the local zone (offset_east is ignored then). */
static void
picorb_time_fields(time_t unixtime, bool utc, bool fixed, long offset_east,
                   struct tm *tm, long *timezone_west)
{
  if (utc || fixed) {
    time_t shifted = unixtime + (utc ? 0 : offset_east);
    gmtime_r(&shifted, tm);
    *timezone_west = utc ? 0 : -offset_east;
    return;
  }
#if defined(PICORB_PLATFORM_POSIX)
  localtime_r(&unixtime, tm);
  *timezone_west = timezone;
#else
  *timezone_west = ENV_get_timezone_offset();
  time_t local_unixtime = unixtime - *timezone_west;
  gmtime_r(&local_unixtime, tm);
#endif
}

/* The offset in seconds east of UTC that a zone argument of Time.new
 * stands for: "+09:00", "-0500", "UTC", "Z", or an Integer. Returns
 * false on anything else. *utc tells whether it named UTC itself. */
static bool
picorb_parse_offset(const char *s, size_t len, long *offset_east, bool *utc)
{
  *utc = false;
  if ((len == 3 && strncmp(s, "UTC", 3) == 0) || (len == 1 && s[0] == 'Z')) {
    *offset_east = 0;
    *utc = true;
    return true;
  }
  if (len < 3 || (s[0] != '+' && s[0] != '-')) return false;
  int sign = s[0] == '-' ? -1 : 1;
  int h = 0, m = 0, sec = 0;
  size_t i = 1;
  if (i + 2 > len) return false;
  h = (s[i] - '0') * 10 + (s[i + 1] - '0');
  i += 2;
  if (i < len && s[i] == ':') i++;
  if (i + 2 <= len) {
    m = (s[i] - '0') * 10 + (s[i + 1] - '0');
    i += 2;
    if (i < len && s[i] == ':') i++;
    if (i + 2 <= len) {
      sec = (s[i] - '0') * 10 + (s[i + 1] - '0');
      i += 2;
    }
  }
  if (i != len || h > 23 || m > 59 || sec > 59) return false;
  *offset_east = sign * (h * 3600 + m * 60 + sec);
  return true;
}

/* The zone abbreviation of a local time, or NULL when the platform does
 * not know it */
static const char *
picorb_local_zone(const struct tm *tm)
{
#if defined(PICORB_PLATFORM_POSIX) && !defined(_WIN32)
  return tm->tm_zone;
#else
  (void)tm;
  return NULL;
#endif
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/time.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/time.c"

#endif
