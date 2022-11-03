#ifndef FLASH_DISK_DEFINED
#define FLASH_DISK_DEFINED

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"

#ifdef __cplusplus
extern "C" {
#endif

int FLASH_disk_initialize(void);
int FLASH_disk_status(void);
int FLASH_disk_read(BYTE *buff, LBA_t sector, UINT count);
int FLASH_disk_write(const BYTE *buff, LBA_t sector, UINT count);
DRESULT FLASH_disk_ioctl(BYTE cmd, int *buff);
DWORD get_fattime(void);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_DISK_DEFINED */

