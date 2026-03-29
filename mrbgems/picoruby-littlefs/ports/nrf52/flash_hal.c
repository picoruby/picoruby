#include <stdbool.h>
#include <string.h>

#include "nrf_fstorage.h"
#include "nrf_fstorage_nvmc.h"

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

static void
fstorage_evt_handler(nrf_fstorage_evt_t *p_evt)
{
  (void)p_evt;
}

NRF_FSTORAGE_DEF(nrf_fstorage_t littlefs_fstorage) = {
  .evt_handler = fstorage_evt_handler,
  .start_addr = LFS_FLASH_TARGET_OFFSET,
  .end_addr = LFS_FLASH_TARGET_OFFSET + (LFS_FLASH_BLOCK_SIZE * LFS_FLASH_BLOCK_COUNT),
};

static bool littlefs_fstorage_initialized = false;

static void
littlefs_hal_init_fstorage(void)
{
  if (littlefs_fstorage_initialized) {
    return;
  }

  ret_code_t rc = nrf_fstorage_init(&littlefs_fstorage, &nrf_fstorage_nvmc, NULL);
  if (rc == NRF_SUCCESS) {
    littlefs_fstorage_initialized = true;
  }
}

static int
lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
               lfs_off_t off, void *buffer, lfs_size_t size)
{
  const uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size + off;
  littlefs_hal_init_fstorage();
  ret_code_t rc = nrf_fstorage_read(&littlefs_fstorage, addr, buffer, size);
  return rc == NRF_SUCCESS ? LFS_ERR_OK : LFS_ERR_IO;
}

static int
lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
               lfs_off_t off, const void *buffer, lfs_size_t size)
{
  uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size + off;
  littlefs_hal_init_fstorage();
  ret_code_t rc = nrf_fstorage_write(&littlefs_fstorage, addr, buffer, size, NULL);
  while (nrf_fstorage_is_busy(&littlefs_fstorage)) {}
  return rc == NRF_SUCCESS ? LFS_ERR_OK : LFS_ERR_IO;
}

static int
lfs_flash_erase(const struct lfs_config *c, lfs_block_t block)
{
  uint32_t addr = LFS_FLASH_TARGET_OFFSET + block * c->block_size;
  littlefs_hal_init_fstorage();
  ret_code_t rc = nrf_fstorage_erase(&littlefs_fstorage, addr, 1, NULL);
  while (nrf_fstorage_is_busy(&littlefs_fstorage)) {}
  return rc == NRF_SUCCESS ? LFS_ERR_OK : LFS_ERR_IO;
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
  littlefs_hal_init_fstorage();
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
  littlefs_hal_init_fstorage();
  for (uint32_t i = 0; i < LFS_FLASH_BLOCK_COUNT; i++) {
    (void)nrf_fstorage_erase(&littlefs_fstorage,
      LFS_FLASH_TARGET_OFFSET + (i * LFS_FLASH_BLOCK_SIZE), 1, NULL);
    while (nrf_fstorage_is_busy(&littlefs_fstorage)) {}
  }
}
