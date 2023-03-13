#ifndef SD_DISK_DEFINED
#define SD_DISK_DEFINED

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"

#ifdef __cplusplus
extern "C" {
#endif

int SD_disk_erase(void);
int SD_disk_initialize(void);
int SD_disk_status(void);
int SD_disk_read(BYTE *buff, LBA_t sector, UINT count);
int SD_disk_write(const BYTE *buff, LBA_t sector, UINT count);
DRESULT SD_disk_ioctl(BYTE cmd, int *buff);

#ifdef __cplusplus
}
#endif

#endif /* SD_DISK_DEFINED */

