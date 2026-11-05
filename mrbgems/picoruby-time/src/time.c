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

/* Time.new's String form: "YYYY-MM-DD HH:MM:SS", a 'T' in place of the
 * space, a fraction after the seconds, a zone after a space or right
 * after the seconds ("Z", "+09:00", "-0500", "UTC"), or a year alone.
 * The fields land in tm (which gets no wday or yday) and *usec; *has_zone
 * says whether the string named a zone, and then *utc and *offset_east
 * are what picorb_parse_offset gives. Returns NULL, or the message of
 * the ArgumentError to raise, "can't parse" for a string of another
 * shape. The checks and messages are CRuby's. */
static bool
picorb_two_digits(const char *s, size_t len, size_t *i, int *out)
{
  if (*i + 2 > len || s[*i] < '0' || '9' < s[*i] || s[*i + 1] < '0' || '9' < s[*i + 1]) return false;
  *out = (s[*i] - '0') * 10 + (s[*i + 1] - '0');
  *i += 2;
  return true;
}

static const char *
picorb_parse_time_string(const char *s, size_t len, struct tm *tm, int *usec,
                         bool *has_zone, bool *utc, long *offset_east)
{
  size_t i = 0;
  int n;

  memset(tm, 0, sizeof(*tm));
  tm->tm_mday = 1;
  *usec = 0;
  *has_zone = false;
  *utc = false;
  *offset_east = 0;

  long year = 0;
  size_t start = i;
  while (i < len && '0' <= s[i] && s[i] <= '9') {
    year = year * 10 + (s[i] - '0');
    i++;
  }
  if (i == start) return "can't parse";
  tm->tm_year = (int)(year - 1900);
  if (i == len) return NULL;                    /* a year alone */
  if (s[i] != '-') return "can't parse";
  i++;
  if (!picorb_two_digits(s, len, &i, &n)) return "two digits mon is expected after '-'";
  if (n < 1 || 12 < n) return "mon out of range";
  tm->tm_mon = n - 1;
  if (i == len || s[i] != '-') return "can't parse";
  i++;
  if (!picorb_two_digits(s, len, &i, &n)) return "two digits mday is expected after '-'";
  if (n < 1 || 31 < n) return "mday out of range";
  tm->tm_mday = n;
  if (i == len) return "no time information";
  if (s[i] != ' ' && s[i] != 'T') return "can't parse";
  i++;
  if (!picorb_two_digits(s, len, &i, &n)) return "two digits hour is expected";
  if (23 < n) return "hour out of range";
  tm->tm_hour = n;
  if (i == len || s[i] != ':') return "missing min part";
  i++;
  if (!picorb_two_digits(s, len, &i, &n)) return "two digits min is expected after ':'";
  if (59 < n) return "min out of range";
  tm->tm_min = n;
  if (i == len || s[i] != ':') return "missing sec part";
  i++;
  if (!picorb_two_digits(s, len, &i, &n)) return "two digits sec is expected after ':'";
  if (60 < n) return "sec out of range";
  tm->tm_sec = n;
  if (i < len && s[i] == '.') {
    i++;
    start = i;
    long frac = 0;
    int digits = 0;
    while (i < len && '0' <= s[i] && s[i] <= '9') {
      if (digits < 6) {
        frac = frac * 10 + (s[i] - '0');
        digits++;
      }
      i++;
    }
    if (i == start) return "can't parse";
    while (digits < 6) {
      frac *= 10;
      digits++;
    }
    *usec = (int)frac;
  }
  if (i == len) return NULL;
  if (s[i] == ' ') i++;
  if (i == len) return "can't parse";
  if (!picorb_parse_offset(s + i, len - i, offset_east, utc)) {
    return "\"+HH:MM\", \"-HH:MM\", \"UTC\" or \"A\"..\"I\",\"K\"..\"Z\" expected for utc_offset";
  }
  *has_zone = true;
  return NULL;
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
