#ifndef RAM_DISK_DEFINED
#define RAM_DISK_DEFINED

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Volume size = SECTOR_SIZE * SECTOR_COUNT
 *  512 * 192 == 96 KiB
 *  Seems this is the minimum volume size of FatFS
 */
#define SECTOR_SIZE   512
#define SECTOR_COUNT  192

extern BYTE *ram_disk;

int RAM_disk_erase(void);
int RAM_disk_initialize(void);
int RAM_disk_status(void);
int RAM_disk_read(BYTE *buff, LBA_t sector, UINT count);
int RAM_disk_write(const BYTE *buff, LBA_t sector, UINT count);
DRESULT RAM_disk_ioctl(BYTE cmd, void *buff);

#ifdef __cplusplus
}
#endif

#endif /* RAM_DISK_DEFINED */
