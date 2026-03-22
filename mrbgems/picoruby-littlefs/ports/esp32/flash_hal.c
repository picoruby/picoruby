#include <string.h>

#include "esp_flash.h"

#include "../../lib/littlefs/lfs.h"
#include "../../include/littlefs.h"

#if !defined(LFS_FLASH_TARGET_OFFSET)
  #define LFS_FLASH_TARGET_OFFSET  0x00210000
#endif

#define LFS_FLASH_BLOCK_SIZE   4096  /* == SPI_FLASH_SEC_SIZE */
#define LFS_FLASH_BLOCK_COUNT  256   /* 1 MB */
#define LFS_FLASH_PAGE_SIZE    256

static int
lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
               lfs_off_t off, void *buffer, lfs_size_t size)
{
  uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size + off;
  esp_err_t ret = esp_flash_read(esp_flash_default_chip, buffer, addr, size);
  if (ret != ESP_OK) {
    return LFS_ERR_IO;
  }
  return LFS_ERR_OK;
}

static int
lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
               lfs_off_t off, const void *buffer, lfs_size_t size)
{
  uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size + off;
  esp_err_t ret = esp_flash_write(esp_flash_default_chip, buffer, addr, size);
  if (ret != ESP_OK) {
    return LFS_ERR_IO;
  }
  return LFS_ERR_OK;
}

static int
lfs_flash_erase(const struct lfs_config *c, lfs_block_t block)
{
  uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size;
  esp_err_t ret = esp_flash_erase_region(esp_flash_default_chip,
                                         addr, c->block_size);
  if (ret != ESP_OK) {
    return LFS_ERR_IO;
  }
  return LFS_ERR_OK;
}

static int
lfs_flash_sync(const struct lfs_config *c)
{
  (void)c;
  return LFS_ERR_OK;
}

void
littlefs_hal_init_config(struct lfs_config *cfg)
{
  memset(cfg, 0, sizeof(struct lfs_config));
  cfg->read  = lfs_flash_read;
  cfg->prog  = lfs_flash_prog;
  cfg->erase = lfs_flash_erase;
  cfg->sync  = lfs_flash_sync;

  cfg->read_size      = LFS_FLASH_PAGE_SIZE;
  cfg->prog_size      = LFS_FLASH_PAGE_SIZE;
  cfg->block_size     = LFS_FLASH_BLOCK_SIZE;
  cfg->block_count    = LFS_FLASH_BLOCK_COUNT;
  cfg->block_cycles   = 500;
  cfg->cache_size     = LFS_FLASH_PAGE_SIZE;
  cfg->lookahead_size = 16;
}

void
littlefs_hal_erase_all(void)
{
  esp_flash_erase_region(
    esp_flash_default_chip,
    (uint32_t)LFS_FLASH_TARGET_OFFSET,
    (size_t)(LFS_FLASH_BLOCK_SIZE * LFS_FLASH_BLOCK_COUNT)
  );
}
