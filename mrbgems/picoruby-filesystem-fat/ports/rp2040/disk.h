#ifndef DISK_DEFINED_H_
#define DISK_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FLASH_SECTOR_SIZE  4096
#define FLASH_SECTOR_COUNT  192 /* Seems FatFS allows 192 as the minimum */

#define SD_SECTOR_SIZE      512

#if defined(PICORUBY_MSC_FLASH)
  /* 4096 * 192 = 768 KiB */
  #define MSC_SECTOR_SIZE     FLASH_SECTOR_SIZE
  #define MSC_SECTOR_COUNT    FLASH_SECTOR_SIZE
#elif defined(PICORUBY_MSC_SD)
  #define MSC_SECTOR_SIZE     SD_SECTOR_SIZE
#endif

#ifdef __cplusplus
}
#endif

#endif /* DISK_DEFINED_H_ */

