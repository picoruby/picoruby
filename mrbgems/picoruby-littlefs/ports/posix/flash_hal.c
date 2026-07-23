#include <stdlib.h>
#include <string.h>

#include "../../lib/littlefs/lfs.h"
#include "../../include/littlefs.h"

/*
 * RAM backed block device for host builds.
 *
 * There is no flash on the host, so the volume lives in a malloc'd buffer and
 * is gone when the process exits. It behaves like the real thing otherwise
 * (erase leaves 0xff behind, programming only clears bits), which is enough to
 * exercise littlefs and anything layered on top of it, such as
 * picoruby-sqlite3, without a board attached.
 */

#if !defined(LFS_RAM_BLOCK_SIZE)
#define LFS_RAM_BLOCK_SIZE   4096
#endif
#if !defined(LFS_RAM_BLOCK_COUNT)
#define LFS_RAM_BLOCK_COUNT  128   /* 512 KB */
#endif
#define LFS_RAM_PAGE_SIZE    256
#define LFS_RAM_TOTAL_SIZE   ((size_t)LFS_RAM_BLOCK_SIZE * LFS_RAM_BLOCK_COUNT)

static uint8_t *lfs_ram_storage = NULL;

static uint8_t *
lfs_ram_ensure_storage(void)
{
  if (lfs_ram_storage == NULL) {
    lfs_ram_storage = (uint8_t *)malloc(LFS_RAM_TOTAL_SIZE);
    if (lfs_ram_storage == NULL) return NULL;
    memset(lfs_ram_storage, 0xff, LFS_RAM_TOTAL_SIZE);
  }
  return lfs_ram_storage;
}

static int
lfs_ram_read(const struct lfs_config *c, lfs_block_t block,
             lfs_off_t off, void *buffer, lfs_size_t size)
{
  uint8_t *storage = lfs_ram_ensure_storage();
  if (storage == NULL) return LFS_ERR_IO;
  memcpy(buffer, storage + (size_t)block * c->block_size + off, size);
  return LFS_ERR_OK;
}

static int
lfs_ram_prog(const struct lfs_config *c, lfs_block_t block,
             lfs_off_t off, const void *buffer, lfs_size_t size)
{
  uint8_t *storage = lfs_ram_ensure_storage();
  if (storage == NULL) return LFS_ERR_IO;
  uint8_t *dst = storage + (size_t)block * c->block_size + off;
  const uint8_t *src = (const uint8_t *)buffer;
  lfs_size_t i = 0;
  while (i < size) {
    /* NOR flash can only clear bits; keep the same failure mode here */
    dst[i] &= src[i];
    i++;
  }
  return LFS_ERR_OK;
}

static int
lfs_ram_erase(const struct lfs_config *c, lfs_block_t block)
{
  uint8_t *storage = lfs_ram_ensure_storage();
  if (storage == NULL) return LFS_ERR_IO;
  memset(storage + (size_t)block * c->block_size, 0xff, c->block_size);
  return LFS_ERR_OK;
}

static int
lfs_ram_sync(const struct lfs_config *c)
{
  (void)c;
  return LFS_ERR_OK;
}

void
littlefs_hal_init_config(struct lfs_config *cfg)
{
  memset(cfg, 0, sizeof(struct lfs_config));
  cfg->read  = lfs_ram_read;
  cfg->prog  = lfs_ram_prog;
  cfg->erase = lfs_ram_erase;
  cfg->sync  = lfs_ram_sync;

  cfg->read_size      = LFS_RAM_PAGE_SIZE;
  cfg->prog_size      = LFS_RAM_PAGE_SIZE;
  cfg->block_size     = LFS_RAM_BLOCK_SIZE;
  cfg->block_count    = LFS_RAM_BLOCK_COUNT;
  cfg->block_cycles   = 500;
  cfg->cache_size     = LFS_RAM_PAGE_SIZE;
  cfg->lookahead_size = 16;
}

void
littlefs_hal_erase_all(void)
{
  uint8_t *storage = lfs_ram_ensure_storage();
  if (storage == NULL) return;
  memset(storage, 0xff, LFS_RAM_TOTAL_SIZE);
}
