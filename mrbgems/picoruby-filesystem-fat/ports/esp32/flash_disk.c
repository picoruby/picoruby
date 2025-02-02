#include <stdlib.h>
#include <string.h>

#include "esp_flash.h"
#include "spi_flash_mmap.h"

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"

#define BLOCK_SIZE   (16 * SPI_FLASH_SEC_SIZE)
#define FLASH_SIZE   (1024 * 1024)
#define FLASH_OFFSET (0x110000)

static void *mapped_addr = NULL;
static spi_flash_mmap_handle_t handle = 0;

int FLASH_disk_erase(void) {
  esp_flash_t *chip = esp_flash_default_chip;

  esp_err_t ret = esp_flash_erase_chip(chip);
  if(ret != ESP_OK) {
    return RES_ERROR;
  }

  return RES_OK;
}

int FLASH_disk_initialize(void) {
  if (handle == 0) {
    esp_err_t ret = spi_flash_mmap(
      FLASH_OFFSET,
      FLASH_SIZE,
      SPI_FLASH_MMAP_DATA,
      (const void**)&mapped_addr,
      &handle
    );
    if(ret != ESP_OK) {
      return STA_NOINIT;
    }
  }

  return RES_OK;
}

int FLASH_disk_status(void) {
  /* Flash ROM is always ready */
  return RES_OK;
}

int FLASH_disk_read(BYTE *buff, LBA_t sector, UINT count) {
  esp_flash_t *chip = esp_flash_default_chip;
  uint32_t offset = FLASH_OFFSET + (sector * SPI_FLASH_SEC_SIZE);
  uint32_t size = count * SPI_FLASH_SEC_SIZE;

  esp_err_t ret = esp_flash_read(chip, buff, offset, size);
  if(ret != ESP_OK) {
    return RES_ERROR;
  }

  return RES_OK;
}

int FLASH_disk_write(const BYTE *buff, LBA_t sector, UINT count) {
  esp_flash_t *chip = esp_flash_default_chip;
  uint32_t offset = FLASH_OFFSET + (sector * SPI_FLASH_SEC_SIZE);
  uint32_t size = count * SPI_FLASH_SEC_SIZE;

  esp_err_t ret = esp_flash_erase_region(chip, offset, size);
  if(ret != ESP_OK) {
    return RES_ERROR;
  }

  ret = esp_flash_write(chip, buff, offset, size);
  if(ret != ESP_OK) {
    return RES_ERROR;
  }

  return RES_OK;
}

DRESULT FLASH_disk_ioctl(BYTE cmd, void *buff) {
  switch (cmd) {
    case CTRL_SYNC:
      // TODO
      break;
    case GET_BLOCK_SIZE:
      *((DWORD *)buff) = (DWORD)BLOCK_SIZE;
      break;
    case CTRL_TRIM:
      // TODO
      return RES_ERROR;
    case GET_SECTOR_SIZE:
      *((WORD *)buff) = (WORD)SPI_FLASH_SEC_SIZE;
      break;
    case GET_SECTOR_COUNT:
      *((LBA_t *)buff) = (LBA_t)(FLASH_SIZE / SPI_FLASH_SEC_SIZE);
      break;
    default :
      return RES_PARERR;
  }
  return RES_OK;
}

static LBA_t clst2sect (	/* !=0:Sector number, 0:Failed (invalid cluster#) */
	FATFS* fs,		/* Filesystem object */
	DWORD clst		/* Cluster# to be converted */
) {
	clst -= 2;		/* Cluster number is origin from 2 */
	if (clst >= fs->n_fatent - 2) return 0;		/* Is it invalid cluster number? */
	return fs->database + (LBA_t)fs->csize * clst;	/* Start sector number of the cluster */
}

void FILE_physical_address(FIL *fp, uint8_t **addr) {
  FATFS *fs = fp->obj.fs;
  LBA_t sect;

  sect = clst2sect(fs, fp->obj.sclust);

  *addr = (uint8_t*)(mapped_addr + sect * SPI_FLASH_SEC_SIZE);
}

int FILE_sector_size(void) {
  return SPI_FLASH_SEC_SIZE;
}
