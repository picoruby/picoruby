#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "../include/fat.h"

#include "../lib/ff14b/source/ff.h"
#include "../lib/ff14b/source/ffconf.h"

#if FF_STR_VOLUME_ID
#ifdef FF_VOLUME_STRS
static const char* const VolumeStr[FF_VOLUMES] = {FF_VOLUME_STRS};	/* Pre-defined volume ID */
#endif
#endif

#include "hal/diskio.h"

static time_t unixtime_offset = 0;

DWORD
get_fattime(void)
{
  time_t unixtime = time(NULL) + unixtime_offset;
  struct tm local;
  localtime_r(&unixtime, &local);
  DWORD time = (local.tm_year + 1900 - 1980) << 25
               | (local.tm_mon + 1) << 21
               | local.tm_mday << 16
               | local.tm_hour << 11
               | local.tm_min << 5
               | local.tm_sec / 2;
  return time;
}


static void
unixtime2fno(const time_t *unixtime, FILINFO *fno)
{
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
  struct tm localtime;
  localtime_r(unixtime, &localtime);
  fno->fdate = (WORD)(((localtime.tm_year + 1900 - 1980) << 9) | (localtime.tm_mon + 1) << 5 | localtime.tm_mday);
  fno->ftime = (WORD)(localtime.tm_hour << 11 | localtime.tm_min << 5 | localtime.tm_sec / 2);
#else
  struct tm *local = localtime(unixtime);
  fno->fdate = (WORD)(((local->tm_year + 1900 - 1980) << 9) | (local->tm_mon + 1) << 5 | local->tm_mday);
  fno->ftime = (WORD)(local->tm_hour << 11 | local->tm_min << 5 | local->tm_sec / 2);
#endif
}

static time_t
fno2unixtime(const FILINFO *fno)
{
  struct tm tm;
  tm.tm_year = (fno->fdate >> 9) + 1980 - 1900;
  tm.tm_mon  = ((fno->fdate >> 5) & 15) - 1;
  tm.tm_mday = (fno->fdate & 31);
  tm.tm_hour = (fno->ftime >> 11);
  tm.tm_min  = (fno->ftime >> 5) & 63;
  tm.tm_sec  = (fno->ftime & 31) * 2;
  tm.tm_isdst = 0;
  return mktime(&tm);
}

int
FAT_prepare_exception(FRESULT res, char *buff, const char *func)
{
#define PREPARE_EXCEPTION(message) (sprintf(buff, "%s @ %s", message, func))
  switch (res) {
    case FR_OK:
      return 0;
    case FR_DISK_ERR:
      PREPARE_EXCEPTION("Unrecoverable hard error");
      break;
    case FR_INT_ERR:
      PREPARE_EXCEPTION("Insanity is detected");
      break;
    case FR_NOT_READY:
      PREPARE_EXCEPTION("Storage device not ready");
      break;
    case FR_NO_FILE:
    case FR_NO_PATH:
      PREPARE_EXCEPTION("No such file or directory");
      break;
    case FR_INVALID_NAME:
      PREPARE_EXCEPTION("Invalid as a path name");
      break;
    case FR_DENIED:
      PREPARE_EXCEPTION("Permission denied");
      break;
    case FR_EXIST:
      PREPARE_EXCEPTION("File exists");
      break;
    case FR_INVALID_OBJECT:
      PREPARE_EXCEPTION("Invalid object");
      break;
    case FR_WRITE_PROTECTED:
      PREPARE_EXCEPTION("Write protected");
      break;
    case FR_INVALID_DRIVE:
      PREPARE_EXCEPTION("Invalid drive number");
      break;
    case FR_NOT_ENABLED:
      PREPARE_EXCEPTION("Drive not mounted");
      break;
    case FR_NO_FILESYSTEM:
      PREPARE_EXCEPTION("No valid FAT volume found");
      break;
    case FR_MKFS_ABORTED:
      PREPARE_EXCEPTION("Volume format could not start");
      break;
    case FR_TIMEOUT:
      PREPARE_EXCEPTION("Timeout");
      break;
    case FR_LOCKED:
      PREPARE_EXCEPTION("Object locked");
      break;
    case FR_NOT_ENOUGH_CORE:
      PREPARE_EXCEPTION("No enough memory");
      break;
    case FR_TOO_MANY_OPEN_FILES:
      PREPARE_EXCEPTION("Too many open files");
      break;
    case FR_INVALID_PARAMETER:
      PREPARE_EXCEPTION("Invalid parameter");
      break;
    default:
      PREPARE_EXCEPTION("This should not happen");
  }
  return -1;
#undef PREPARE_EXCEPTION
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/fat.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/fat.c"

#endif
