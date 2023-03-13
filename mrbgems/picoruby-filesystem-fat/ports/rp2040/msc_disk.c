/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Modification Copyright (c) 2022 HASUMI Hitoshi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <bsp/board.h>
#include <tusb.h>

#include <mrubyc.h>

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"
#include "disk.h"

#ifndef PICORUBY_NO_MSC

#if defined(PICORUBY_MSC_FLASH)
  #include <hardware/flash.h>
  /* 4096 * 192 = 768 KiB */
  #define MSC_SECTOR_SIZE     FLASH_SECTOR_SIZE
  #define MSC_SECTOR_COUNT    FLASH_SECTOR_COUNT
  int FLASH_disk_read(void *buff, int32_t sector, int count);
  int FLASH_disk_write(const void *buff, int32_t sector, int count);
  #define DISK_READ(buff, sector, count)    FLASH_disk_read(buff, sector, count)
  #define DISK_WRITE(buff, sector, count)   FLASH_disk_write(buff, sector, count)
#elif defined(PICORUBY_MSC_SD)
  #define MSC_SECTOR_SIZE     SD_SECTOR_SIZE
  int SD_disk_status(void);
  int SD_disk_ioctl(unsigned char cmd, void *buff);
  int SD_disk_read(void *buff, int32_t sector, int count);
  int SD_disk_write(const void *buff, int32_t sector, int count);
  #define DISK_READ(buff, sector, count)    SD_disk_read(buff, sector, count)
  #define DISK_WRITE(buff, sector, count)   SD_disk_write(buff, sector, count)
#endif

// whether host does safe-eject
static bool ejected = false;

static bool
check_sector_count(uint32_t lba)
{
#if defined(PICORUBY_MSC_FLASH)
  if (lba >= MSC_SECTOR_COUNT) return false;
#elif defined(PICORUBY_MSC_SD)
  uint16_t block_count = 0;
  if (SD_disk_ioctl(GET_SECTOR_COUNT, &block_count)) return false;
  if (lba >= block_count) return false;
#else
  #error
#endif
  return true;
}

// Invoked when received SCSI_CMD_INQUIRY
// Application fill vendor id, product id and revision with string up to 8, 16, 4 characters respectively
void
tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4])
{
  const char vid[] = "PicoRuby";
  const char pid[] = "PicoRuby MSC";
  const char rev[] = "1.0";
  memcpy(vendor_id  , vid, strlen(vid));
  memcpy(product_id , pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
bool
tud_msc_test_unit_ready_cb(uint8_t lun)
{
#if defined(PICORUBY_MSC_SD)
  if (0 < (SD_disk_status() & (STA_NOINIT|STA_NODISK))) {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }
#endif
  if (ejected) {
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
    return false;
  }
  return true;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and SCSI_CMD_READ_FORMAT_CAPACITY to determine the disk size
// Application update block count and block size
void
tud_msc_capacity_cb(uint8_t lun, uint32_t* block_count, uint16_t* block_size)
{
#if defined(PICORUBY_MSC_FLASH)
  *block_count = MSC_SECTOR_COUNT;
#elif defined(PICORUBY_MSC_SD)
  SD_disk_ioctl(GET_SECTOR_COUNT, block_count);
#endif
  *block_size  = MSC_SECTOR_SIZE;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool
tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start, bool load_eject)
{
  if (load_eject) {
    if (start) {
      // load disk storage
    } else {
      // unload disk storage
      ejected = true;
    }
  }
  return true;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.

int32_t
tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize)
{
  if (!check_sector_count(lba)) return -1;
  /*
   * In order to avoid (0 < offset) and (bufsize != MSC_SECTOR_SIZE),
   * CFG_TUD_MSC_EP_BUFSIZE has to be equal to MSC_SECTOR_SIZE.
   */
  if (0 < offset || bufsize != MSC_SECTOR_SIZE) {
    console_printf("Failed in tud_msc_read10_cb()\n");
    console_printf("offset: %d, bufsize: %d\n", offset, bufsize);
    tud_task();
    abort();
  }

  DISK_READ(buffer, lba, 1);
  return MSC_SECTOR_SIZE;
}

bool
tud_msc_is_writable_cb(uint8_t lun)
{
#ifdef PICORUBY_MSC_READONLY
  return false;
#else
  return true;
#endif
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
int32_t
tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize)
{
  if (!check_sector_count(lba)) return -1;
  /*
   * In order to avoid (0 < offset) and (bufsize != MSC_SECTOR_SIZE),
   * CFG_TUD_MSC_EP_BUFSIZE has to be equal to MSC_SECTOR_SIZE.
   */
  if (0 < offset || bufsize != MSC_SECTOR_SIZE) {
    console_printf("Failed in tud_msc_write10_cb()\n");
    console_printf("offset: %d, bufsize: %d\n", offset, bufsize);
    tud_task();
    abort();
  }

  DISK_WRITE(buffer, lba, 1);
  return MSC_SECTOR_SIZE;
}

// Callback invoked when received an SCSI command not in built-in list below
// - READ_CAPACITY10, READ_FORMAT_CAPACITY, INQUIRY, MODE_SENSE6, REQUEST_SENSE
// - READ10 and WRITE10 has their own callbacks
int32_t
tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void* buffer, uint16_t bufsize)
{
  // read10 & write10 has their own callback and MUST not be handled here
  void const* response = NULL;
  int32_t resplen = 0;
  // most scsi handled is input
  bool in_xfer = true;
  switch (scsi_cmd[0]) {
    case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
      // Host is about to read/write etc ... better not to disconnect disk
      resplen = 0;
      break;
    default:
      // Set Sense = Invalid Command Operation
      tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
      // negative means error -> tinyusb could stall and/or response with failed status
      resplen = -1;
      break;
  }

  // return resplen must not larger than bufsize
  if (resplen > bufsize) resplen = bufsize;
  if (response && (resplen > 0)) {
    if (in_xfer) {
      memcpy(buffer, response, resplen);
    } else {
      // SCSI output
    }
  }
  return resplen;
}

#endif /* PICORUBY_NO_MSC */

