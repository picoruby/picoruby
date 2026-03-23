#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "../include/littlefs.h"

static lfs_t lfs;
static struct lfs_config lfs_cfg;
static bool lfs_mounted = false;

static time_t unixtime_offset = 0;

lfs_t *
littlefs_get_lfs(void)
{
  return &lfs;
}

struct lfs_config *
littlefs_get_config(void)
{
  return &lfs_cfg;
}

int
littlefs_ensure_mounted(void)
{
  if (lfs_mounted) return 0;
  littlefs_hal_init_config(&lfs_cfg);
  int err = lfs_mount(&lfs, &lfs_cfg);
  if (err == LFS_ERR_CORRUPT) {
    /* Flash not formatted yet; format and retry */
    err = lfs_format(&lfs, &lfs_cfg);
    if (err != LFS_ERR_OK) return err;
    err = lfs_mount(&lfs, &lfs_cfg);
  }
  if (err == LFS_ERR_OK) {
    lfs_mounted = true;
  }
  return err;
}

uint32_t
littlefs_get_unixtime(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint32_t)(tv.tv_sec + unixtime_offset);
}

int
LFS_prepare_exception(int err, char *buff, const char *func)
{
#define PREPARE_EXCEPTION(message) (sprintf(buff, "%s @ %s", message, func))
  switch (err) {
    case LFS_ERR_OK:
      return 0;
    case LFS_ERR_IO:
      PREPARE_EXCEPTION("Error during device operation");
      break;
    case LFS_ERR_CORRUPT:
      PREPARE_EXCEPTION("Corrupted");
      break;
    case LFS_ERR_NOENT:
      PREPARE_EXCEPTION("No such file or directory");
      break;
    case LFS_ERR_EXIST:
      PREPARE_EXCEPTION("Entry already exists");
      break;
    case LFS_ERR_NOTDIR:
      PREPARE_EXCEPTION("Entry is not a directory");
      break;
    case LFS_ERR_ISDIR:
      PREPARE_EXCEPTION("Entry is a directory");
      break;
    case LFS_ERR_NOTEMPTY:
      PREPARE_EXCEPTION("Directory is not empty");
      break;
    case LFS_ERR_BADF:
      PREPARE_EXCEPTION("Bad file number");
      break;
    case LFS_ERR_FBIG:
      PREPARE_EXCEPTION("File too large");
      break;
    case LFS_ERR_INVAL:
      PREPARE_EXCEPTION("Invalid parameter");
      break;
    case LFS_ERR_NOSPC:
      PREPARE_EXCEPTION("No space left on device");
      break;
    case LFS_ERR_NOMEM:
      PREPARE_EXCEPTION("No more memory available");
      break;
    case LFS_ERR_NOATTR:
      PREPARE_EXCEPTION("No data/attr available");
      break;
    case LFS_ERR_NAMETOOLONG:
      PREPARE_EXCEPTION("File name too long");
      break;
    default:
      PREPARE_EXCEPTION("Unknown error");
  }
  return -1;
#undef PREPARE_EXCEPTION
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/littlefs.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/littlefs.c"

#endif
