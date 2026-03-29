#include <string.h>

#include "nrf_nvmc.h"

#include "../../lib/littlefs/lfs.h"
#include "../../include/littlefs.h"

#if !defined(LFS_FLASH_TARGET_OFFSET)
  #define LFS_FLASH_TARGET_OFFSET  0x000B4000
#endif

#define LFS_FLASH_BLOCK_SIZE   4096
#define LFS_FLASH_BLOCK_COUNT  64
#define LFS_FLASH_PROG_SIZE    4
#define LFS_FLASH_READ_SIZE    16
#define LFS_FLASH_CACHE_SIZE   64

static int
lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
               lfs_off_t off, void *buffer, lfs_size_t size)
{
  const uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size + off;
  memcpy(buffer, (const void *)addr, size);
  return LFS_ERR_OK;
}

static int
lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
               lfs_off_t off, const void *buffer, lfs_size_t size)
{
  uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size + off;
  nrf_nvmc_write_bytes(addr, buffer, size);
  return LFS_ERR_OK;
}

static int
lfs_flash_erase(const struct lfs_config *c, lfs_block_t block)
{
  uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size;
  nrf_nvmc_page_erase(addr);
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

  cfg->read_size      = LFS_FLASH_READ_SIZE;
  cfg->prog_size      = LFS_FLASH_PROG_SIZE;
  cfg->block_size     = LFS_FLASH_BLOCK_SIZE;
  cfg->block_count    = LFS_FLASH_BLOCK_COUNT;
  cfg->block_cycles   = 500;
  cfg->cache_size     = LFS_FLASH_CACHE_SIZE;
  cfg->lookahead_size = 16;
}

void
littlefs_hal_erase_all(void)
{
  for (uint32_t i = 0; i < LFS_FLASH_BLOCK_COUNT; i++) {
    nrf_nvmc_page_erase(LFS_FLASH_TARGET_OFFSET + (i * LFS_FLASH_BLOCK_SIZE));
  }
}
