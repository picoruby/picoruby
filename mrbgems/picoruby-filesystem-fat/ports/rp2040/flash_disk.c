#include <stdlib.h>
#include <string.h>

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"

#include <hardware/sync.h>
#include <hardware/flash.h>

//#include "../../include/flash_disk.h"

/*
 * 1280 KiB + 768 KiB == 2 MiB
 * Fully exhausts Raspberry Pi Pico ROM because it's 2 MiB.
 * (Other RP2040 board may have a bigger ROM)
 */
#define FLASH_TARGET_OFFSET  0x00140000  /* 1280 KiB for program code */
#define FLASH_MMAP_ADDR      (XIP_BASE + FLASH_TARGET_OFFSET)
/* 4096 * 192 = 768 KiB */
#define SECTOR_SIZE          4096 /* == FLASH_SECTOR_SIZE */
#define SECTOR_COUNT         192  /* Seems FatFS allows 192 as the minimum */

/* Disk Status Bits (DSTATUS) */
#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */
#define STA_PROTECT		0x04	/* Write protected */

int
FLASH_disk_erase(void)
{
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(
    (uint32_t)(FLASH_TARGET_OFFSET),
    (size_t)(SECTOR_SIZE * SECTOR_COUNT)
  );
  restore_interrupts(ints);
  return 0;
}

int
FLASH_disk_initialize(void)
{
  /* Flash ROM is always ready */
  return 0;
}

int
FLASH_disk_status(void)
{
  /* Flash ROM is always ready */
  return 0;
}

int
FLASH_disk_read(BYTE *buff, LBA_t sector, UINT count)
{
  memcpy(
    buff,
    (uint8_t*)(FLASH_MMAP_ADDR + sector * SECTOR_SIZE),
    count * SECTOR_SIZE
  );
  return 0;
}

int
FLASH_disk_write(const BYTE *buff, LBA_t sector, UINT count)
{
  /*
   * flash_range_program() knows XIP_BASE,
   * so you just point FLASH_TARGET_OFFSET
   */
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(
    (uint32_t)(FLASH_TARGET_OFFSET + sector * SECTOR_SIZE),
    (size_t)(SECTOR_SIZE * count)
  );
  flash_range_program(
    (uint32_t)(FLASH_TARGET_OFFSET + sector * SECTOR_SIZE),
    (const uint8_t *)buff,
    (size_t)(SECTOR_SIZE * count)
  );
  restore_interrupts(ints);
  return 0;
}

DRESULT
FLASH_disk_ioctl(BYTE cmd, void *buff)
{
  switch (cmd) {
    case CTRL_SYNC:
      // TODO
      break;
    case GET_BLOCK_SIZE:
      /*
       * According to the FatFS document, GET_BLOCK_SIZE should
       * be a value like this:
       *  *((DWORD *)buff) = (DWORD)(FLASH_SECTOR_SIZE); // 4096
       * However, FR_MKFS_ABORTED happens in f_mkfs with this value.
       */
      *((DWORD *)buff) = (DWORD)(FLASH_SECTOR_SIZE / FLASH_SECTOR_SIZE);
      //*((DWORD *)buff) = (DWORD)(SECTOR_SIZE);
      break;
    case CTRL_TRIM:
      // TODO *((LBA_t *)buff) = (LBA_t)something
      return RES_ERROR;
    case GET_SECTOR_SIZE:
      *((WORD *)buff) = (WORD)SECTOR_SIZE;
      break;
    case GET_SECTOR_COUNT:
      *((LBA_t *)buff) = (LBA_t)SECTOR_COUNT;
      break;
    default :
      return RES_PARERR;
  }
  return RES_OK;
}
