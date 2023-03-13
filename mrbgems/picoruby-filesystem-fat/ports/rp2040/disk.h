#ifndef DISK_DEFINED_H_
#define DISK_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 1280 KiB + 768 KiB == 2 MiB
 * Fully exhausts Raspberry Pi Pico ROM because it's 2 MiB.
 * (Other RP2040 board may have a bigger ROM)
 */
#define FLASH_TARGET_OFFSET  0x00140000  /* 1280 KiB for program code */
#define FLASH_MMAP_ADDR      (XIP_BASE + FLASH_TARGET_OFFSET)
//#define FLASH_SECTOR_SIZE  4096  /* Already defined in hardware/flash.h */
#define FLASH_SECTOR_COUNT  192 /* Seems FatFS allows 192 as the minimum */

#define SD_SECTOR_SIZE      512
/* SD SECTOR COUNT is dynamically decided by SD_disk_ioctl() */

#ifdef __cplusplus
}
#endif

#endif /* DISK_DEFINED_H_ */

