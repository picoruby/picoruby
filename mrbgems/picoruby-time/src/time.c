#include <stdlib.h>
#include <stdio.h>
#include "../include/picogem_time.h"

#if !defined(PICORB_PLATFORM_POSIX)
static long
calculate_timezone_offset(time_t unixtime)
{
  struct tm local_tm, utc_tm;
  localtime_r(&unixtime, &local_tm);
  gmtime_r(&unixtime, &utc_tm);

  /* Calculate the difference in seconds between local time and UTC */
  /* Note: mktime() interprets tm as local time, so we need to be careful */
  long offset = 0;

  /* Calculate hour and minute differences */
  offset += (utc_tm.tm_hour - local_tm.tm_hour) * 3600;
  offset += (utc_tm.tm_min - local_tm.tm_min) * 60;

  /* Handle day boundary crossing */
  if (utc_tm.tm_mday != local_tm.tm_mday) {
    if (utc_tm.tm_mday > local_tm.tm_mday || (utc_tm.tm_mday == 1 && local_tm.tm_mday > 25)) {
      /* UTC is ahead by one day */
      offset += 86400;
    } else {
      /* UTC is behind by one day */
      offset -= 86400;
    }
  }

  return offset;
}
#endif

#if defined(PICORB_VM_MRUBY)

#include "mruby/time.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/time.c"

#endif

