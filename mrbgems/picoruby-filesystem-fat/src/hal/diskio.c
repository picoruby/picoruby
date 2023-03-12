/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*                Modification Copyright (C) HASUMI Hitoshi, 2023        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "../../lib/ff14b/source/ff.h"
#include "diskio.h"

#include "ram_disk.h"
#define DEV_RAM     0

#ifdef USE_FAT_FLASH_DISK
#include "flash_disk.h"
#define DEV_FLASH   1
#endif

#ifdef USE_FAT_SD_DISK
#include "sd_disk.h"
#define DEV_SD      2
#endif

/*-----------------------------------------------------------------------*/
/* Erase Drive                                                           */
/*-----------------------------------------------------------------------*/

DSTATUS disk_erase (
  BYTE pdrv    /* Physical drive nmuber to identify the drive */
)
{
  DSTATUS stat;
  switch (pdrv) {
  case DEV_RAM :
    stat = RAM_disk_erase();
    return stat;
#ifdef USE_FAT_FLASH_DISK
  case DEV_FLASH :
    stat = FLASH_disk_erase();
    return stat;
#endif
#ifdef USE_FAT_SD_DISK
  case DEV_SD :
    stat = SD_disk_erase();
    return stat;
#endif
  }
  return STA_NOINIT;
}


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
  BYTE pdrv    /* Physical drive nmuber to identify the drive */
)
{
  switch (pdrv) {
  case DEV_RAM :
    return RAM_disk_status();
#ifdef USE_FAT_FLASH_DISK
  case DEV_FLASH :
    return FLASH_disk_status();
#endif
#ifdef USE_FAT_SD_DISK
  case DEV_SD :
    return SD_disk_status();
#endif
  }
  return STA_NOINIT;
}


/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
  BYTE pdrv        /* Physical drive nmuber to identify the drive */
)
{
  DSTATUS stat;
  switch (pdrv) {
  case DEV_RAM :
    stat = RAM_disk_initialize();
    return stat;
#ifdef USE_FAT_FLASH_DISK
  case DEV_FLASH :
    stat = FLASH_disk_initialize();
    return stat;
#endif
#ifdef USE_FAT_SD_DISK
  case DEV_SD :
    stat = SD_disk_initialize();
    return stat;
#endif
  }
  return STA_NOINIT;
}


/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
  BYTE pdrv,    /* Physical drive nmuber to identify the drive */
  BYTE *buff,    /* Data buffer to store read data */
  LBA_t sector,  /* Start sector in LBA */
  UINT count    /* Number of sectors to read */
)
{
  DRESULT res = RES_NOTRDY;
  switch (pdrv) {
  case DEV_RAM :
    res = RAM_disk_read(buff, sector, count);
    return res;
#ifdef USE_FAT_FLASH_DISK
  case DEV_FLASH :
    res = FLASH_disk_read(buff, sector, count);
    return res;
#endif
#ifdef USE_FAT_SD_DISK
  case DEV_SD :
    res = SD_disk_read(buff, sector, count);
    return res;
#endif
  }
  return RES_PARERR;
}


/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
  BYTE pdrv,      /* Physical drive nmuber to identify the drive */
  const BYTE *buff,  /* Data to be written */
  LBA_t sector,    /* Start sector in LBA */
  UINT count      /* Number of sectors to write */
)
{
  DRESULT res = RES_NOTRDY;
  switch (pdrv) {
  case DEV_RAM :
    res = RAM_disk_write(buff, sector, count);
    return res;
#ifdef USE_FAT_FLASH_DISK
  case DEV_FLASH :
    res = FLASH_disk_write(buff, sector, count);
    return res;
#endif
#ifdef USE_FAT_SD_DISK
  case DEV_SD :
    res = SD_disk_write(buff, sector, count);
    return res;
#endif
  }
  return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
  BYTE pdrv,    /* Physical drive nmuber (0..) */
  BYTE cmd,    /* Control code */
  void *buff    /* Buffer to send/receive control data */
)
{
  DRESULT res = RES_NOTRDY;
  switch (pdrv) {
  case DEV_RAM :
    res = RAM_disk_ioctl(cmd, buff);
    return res;
#ifdef USE_FAT_FLASH_DISK
  case DEV_FLASH :
    res = FLASH_disk_ioctl(cmd, buff);
    return res;
#endif
#ifdef USE_FAT_SD_DISK
  case DEV_SD :
    res = SD_disk_ioctl(cmd, buff);
    return res;
#endif
  }
  return RES_PARERR;
}

