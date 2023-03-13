#include <stdlib.h>
#include <string.h>

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"

#include <hardware/sync.h>
#include <hardware/flash.h>

#include "disk.h"

int
FLASH_disk_erase(void)
{
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(
    (uint32_t)(FLASH_TARGET_OFFSET),
    (size_t)(FLASH_SECTOR_SIZE * FLASH_SECTOR_COUNT)
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
    (uint8_t*)(FLASH_MMAP_ADDR + sector * FLASH_SECTOR_SIZE),
    count * FLASH_SECTOR_SIZE
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
  /*
   * Erase unit should be a multiple of FLASH_SECTOR_SIZE (4096 Byte)
   */
  flash_range_erase(
    (uint32_t)(FLASH_TARGET_OFFSET + sector * FLASH_SECTOR_SIZE),
    (size_t)(FLASH_SECTOR_SIZE * count)
  );
  /*
   * Program (Write) unit should be a multiple of FLASH_PAGE_SIZE (256 Byte)
   */
  flash_range_program(
    (uint32_t)(FLASH_TARGET_OFFSET + sector * FLASH_SECTOR_SIZE),
    (const uint8_t *)buff,
    (size_t)(FLASH_SECTOR_SIZE * count)
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
      //*((DWORD *)buff) = (DWORD)(FLASH_SECTOR_SIZE);
      break;
    case CTRL_TRIM:
      // TODO *((LBA_t *)buff) = (LBA_t)something
      return RES_ERROR;
    case GET_SECTOR_SIZE:
      *((WORD *)buff) = (WORD)FLASH_SECTOR_SIZE;
      break;
    case GET_SECTOR_COUNT:
      *((LBA_t *)buff) = (LBA_t)FLASH_SECTOR_COUNT;
      break;
    default :
      return RES_PARERR;
  }
  return RES_OK;
}
