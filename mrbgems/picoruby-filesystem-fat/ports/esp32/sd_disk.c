/*------------------------------------------------------------------------*/
/* ESP32: SD Card Driver (SPI and SDMMC modes)                            */
/*------------------------------------------------------------------------*/
/*
/  Based on RP2040 implementation by ChaN and HASUMI Hitoshi
/  ESP32 port with SDMMC support
/
/ * This software is a free software and there is NO WARRANTY.
/ * No restriction on use. You can use, modify and redistribute it for
/   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
/ * Redistributions of source code must retain the above copyright notice.
/
/-------------------------------------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include "driver/spi_master.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "disk.h"

static const char *TAG = "SD_DISK";

/*-----------------------------------------------------------------------*/
/* Mode selection (SPI or SDMMC)                                         */
/*-----------------------------------------------------------------------*/

typedef enum {
  SD_MODE_NONE = 0,
  SD_MODE_SPI,
  SD_MODE_SDMMC
} sd_mode_t;

static sd_mode_t sd_mode = SD_MODE_NONE;

/*-----------------------------------------------------------------------*/
/* SPI Mode Variables                                                    */
/*-----------------------------------------------------------------------*/

static spi_device_handle_t spi_handle = NULL;
static spi_host_device_t spi_host = SPI2_HOST;
static int SPI_SCK_PIN  = -1;
static int SPI_CIPO_PIN = -1;
static int SPI_COPI_PIN = -1;
static int SPI_CS_PIN   = -1;
static bool spi_bus_initialized = false;

/*-----------------------------------------------------------------------*/
/* SDMMC Mode Variables                                                  */
/*-----------------------------------------------------------------------*/

static sdmmc_card_t *sdmmc_card = NULL;
static bool sdmmc_initialized = false;
static int SDMMC_CLK_PIN = -1;
static int SDMMC_CMD_PIN = -1;
static int SDMMC_D0_PIN = -1;

/*-----------------------------------------------------------------------*/
/* Configuration Functions                                               */
/*-----------------------------------------------------------------------*/

int
FAT_set_spi_unit(const char* name, int sck, int cipo, int copi, int cs)
{
  if (strcmp(name, "ESP32_SPI2_HOST") == 0 || strcmp(name, "ESP32_HSPI_HOST") == 0) {
    spi_host = SPI2_HOST;
  }
#if SOC_SPI_PERIPH_NUM >= 3
  else if (strcmp(name, "ESP32_SPI3_HOST") == 0 || strcmp(name, "ESP32_VSPI_HOST") == 0) {
    spi_host = SPI3_HOST;
  }
#endif
  else {
    ESP_LOGE(TAG, "Invalid SPI unit: %s", name);
    return -1;
  }
  SPI_SCK_PIN  = sck;
  SPI_CIPO_PIN = cipo;
  SPI_COPI_PIN = copi;
  SPI_CS_PIN   = cs;

  sd_mode = SD_MODE_SPI;
  ESP_LOGI(TAG, "SPI mode configured: host=%d, SCK=%d, MISO=%d, MOSI=%d, CS=%d",
           spi_host, SPI_SCK_PIN, SPI_CIPO_PIN, SPI_COPI_PIN, SPI_CS_PIN);
  return 0;
}

int
FAT_set_sdmmc_pins(int clk, int cmd, int d0)
{
  SDMMC_CLK_PIN = clk;
  SDMMC_CMD_PIN = cmd;
  SDMMC_D0_PIN = d0;

  sd_mode = SD_MODE_SDMMC;
  ESP_LOGI(TAG, "SDMMC mode configured: CLK=%d, CMD=%d, D0=%d", clk, cmd, d0);
  return 0;
}

/*-----------------------------------------------------------------------*/
/* FatFS disk interface                                                  */
/*-----------------------------------------------------------------------*/

#include "../../lib/ff14b/source/ff.h"
#include "../../lib/ff14b/source/diskio.h"


/* MMC card type flags (MMC_GET_TYPE) */
#define CT_MMC3     0x01        /* MMC ver 3 */
#define CT_MMC4     0x02        /* MMC ver 4+ */
#define CT_MMC      0x03        /* MMC */
#define CT_SDC1     0x02        /* SDC ver 1 */
#define CT_SDC2     0x04        /* SDC ver 2+ */
#define CT_SDC      0x0C        /* SDC */
#define CT_BLOCK    0x10        /* Block addressing */


/* MMC/SD command */
#define CMD0    (0)         /* GO_IDLE_STATE */
#define CMD1    (1)         /* SEND_OP_COND (MMC) */
#define ACMD41  (0x80+41)   /* SEND_OP_COND (SDC) */
#define CMD8    (8)         /* SEND_IF_COND */
#define CMD9    (9)         /* SEND_CSD */
#define CMD10   (10)        /* SEND_CID */
#define CMD12   (12)        /* STOP_TRANSMISSION */
#define ACMD13  (0x80+13)   /* SD_STATUS (SDC) */
#define CMD16   (16)        /* SET_BLOCKLEN */
#define CMD17   (17)        /* READ_SINGLE_BLOCK */
#define CMD18   (18)        /* READ_MULTIPLE_BLOCK */
#define CMD23   (23)        /* SET_BLOCK_COUNT (MMC) */
#define ACMD23  (0x80+23)   /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24   (24)        /* WRITE_BLOCK */
#define CMD25   (25)        /* WRITE_MULTIPLE_BLOCK */
#define CMD32   (32)        /* ERASE_ER_BLK_START */
#define CMD33   (33)        /* ERASE_ER_BLK_END */
#define CMD38   (38)        /* ERASE */
#define CMD55   (55)        /* APP_CMD */
#define CMD58   (58)        /* READ_OCR */

#define CS_HIGH()   { if (SPI_CS_PIN >= 0) gpio_set_level(SPI_CS_PIN, 1); }
#define CS_LOW()    { if (SPI_CS_PIN >= 0) gpio_set_level(SPI_CS_PIN, 0); }

#define FCLK_FAST() { }
#define FCLK_SLOW() { }

static volatile DSTATUS Stat = STA_NOINIT;  /* Physical drive status */
static volatile UINT Timer1, Timer2;        /* 1kHz decrement timer stopped at zero (disk_timerproc()) */

static BYTE CardType;   /* Card type flags (SPI mode) */

/*=======================================================================*/
/* SPI Mode Implementation                                               */
/*=======================================================================*/

/* Initialize SPI bus and device */
static esp_err_t
spi_init_bus(void)
{
  if (spi_bus_initialized) {
    return ESP_OK;
  }

  if (SPI_SCK_PIN < 0 || SPI_CIPO_PIN < 0 || SPI_COPI_PIN < 0 || SPI_CS_PIN < 0) {
    ESP_LOGE(TAG, "SPI pins not configured");
    return ESP_ERR_INVALID_STATE;
  }

  // Configure CS pin as GPIO output
  gpio_config_t cs_conf = {
    .pin_bit_mask = (1ULL << SPI_CS_PIN),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&cs_conf);
  gpio_set_level(SPI_CS_PIN, 1);  // CS high (deselected)

  spi_bus_config_t buscfg = {
    .mosi_io_num = SPI_COPI_PIN,
    .miso_io_num = SPI_CIPO_PIN,
    .sclk_io_num = SPI_SCK_PIN,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 512 + 8,
  };

  esp_err_t ret = spi_bus_initialize(spi_host, &buscfg, SPI_DMA_CH_AUTO);
  if (ret == ESP_ERR_INVALID_STATE) {
    // SPI bus already initialized by PicoRuby's SPI module - this is OK
    ESP_LOGI(TAG, "SPI bus already initialized, reusing existing bus");
  } else if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  // Start with slow clock for initialization (400kHz)
  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 400000,
    .mode = 0,
    .spics_io_num = -1,  // We control CS manually
    .queue_size = 1,
    .flags = 0,
  };

  ret = spi_bus_add_device(spi_host, &devcfg, &spi_handle);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
    // Don't free the bus if we didn't initialize it
    return ret;
  }

  spi_bus_initialized = true;
  ESP_LOGI(TAG, "SD card SPI device added");
  return ESP_OK;
}

/* Change SPI clock speed */
static void
spi_set_speed(int speed_hz)
{
  if (spi_handle) {
    spi_bus_remove_device(spi_handle);
    spi_handle = NULL;
  }

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = speed_hz,
    .mode = 0,
    .spics_io_num = -1,
    .queue_size = 1,
    .flags = 0,
  };

  spi_bus_add_device(spi_host, &devcfg, &spi_handle);
}

/* Exchange a byte */
static BYTE
xchg_spi(BYTE dat)
{
  uint8_t rx;
  spi_transaction_t t = {
    .length = 8,
    .tx_buffer = &dat,
    .rx_buffer = &rx,
  };
  spi_device_polling_transmit(spi_handle, &t);
  return rx;
}


/* Receive multiple byte */
static void
rcvr_spi_multi(BYTE *buff, UINT btr)
{
  uint8_t dummy = 0xFF;
  for (UINT i = 0; i < btr; i++) {
    spi_transaction_t t = {
      .length = 8,
      .tx_buffer = &dummy,
      .rx_buffer = &buff[i],
    };
    spi_device_polling_transmit(spi_handle, &t);
  }
}


/* Send multiple byte */
static void
xmit_spi_multi(const BYTE *buff, UINT btx)
{
  for (UINT i = 0; i < btx; i++) {
    spi_transaction_t t = {
      .length = 8,
      .tx_buffer = &buff[i],
      .rx_buffer = NULL,
    };
    spi_device_polling_transmit(spi_handle, &t);
  }
}

/* Wait for card ready */
static int
wait_ready(UINT wt)
{
  BYTE d;
  Timer2 = wt;
  do {
    d = xchg_spi(0xFF);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    if (Timer2 > 0) Timer2--;
  } while (d != 0xFF && Timer2);
  return (d == 0xFF) ? 1 : 0;
}

/* Deselect card and release SPI */
static void
deselect(void)
{
  CS_HIGH();
  xchg_spi(0xFF);
}

/* Select card and wait for ready */
static int
select_card(void)
{
  CS_LOW();
  xchg_spi(0xFF);
  if (wait_ready(500)) return 1;
  deselect();
  return 0;
}

/* Receive a data packet from the MMC */
static int
rcvr_datablock(BYTE *buff, UINT btr)
{
  BYTE token;
  Timer1 = 200;
  do {
    token = xchg_spi(0xFF);
    vTaskDelay(1 / portTICK_PERIOD_MS);
    if (Timer1 > 0) Timer1--;
  } while ((token == 0xFF) && Timer1);
  if(token != 0xFE) return 0;

  rcvr_spi_multi(buff, btr);
  xchg_spi(0xFF); xchg_spi(0xFF);

  return 1;
}

/* Send a data packet to the MMC */
static int
xmit_datablock(const BYTE *buff, BYTE token)
{
  BYTE resp;
  if (!wait_ready(500)) return 0;
  xchg_spi(token);
  if (token != 0xFD) {
    xmit_spi_multi(buff, 512);
    xchg_spi(0xFF); xchg_spi(0xFF);

    resp = xchg_spi(0xFF);
    if ((resp & 0x1F) != 0x05) return 0;
  }
  return 1;
}

/* Send a command packet to the MMC */
static BYTE
send_cmd(BYTE cmd, DWORD arg)
{
  BYTE n, res;
  if (cmd & 0x80) {
    cmd &= 0x7F;
    res = send_cmd(CMD55, 0);
    if (res > 1) return res;
  }

  if (cmd != CMD12) {
    deselect();
    if (!select_card()) return 0xFF;
  }

  xchg_spi(0x40 | cmd);
  xchg_spi((BYTE)(arg >> 24));
  xchg_spi((BYTE)(arg >> 16));
  xchg_spi((BYTE)(arg >> 8));
  xchg_spi((BYTE)arg);
  n = 0x01;
  if (cmd == CMD0) n = 0x95;
  if (cmd == CMD8) n = 0x87;
  xchg_spi(n);

  if (cmd == CMD12) xchg_spi(0xFF);
  n = 10;
  do {
    res = xchg_spi(0xFF);
  } while ((res & 0x80) && --n);

  return res;
}

/* SPI mode disk initialize */
static DSTATUS
spi_disk_initialize(void)
{
  BYTE n, cmd, ty, ocr[4];

  ESP_LOGI(TAG, "SPI mode SD_disk_initialize called");

  if (spi_init_bus() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SPI bus");
    return STA_NOINIT;
  }

  if (Stat & STA_NODISK) return Stat;

  FCLK_SLOW();

  // Send 80 dummy clocks with CS high
  CS_HIGH();
  for (n = 10; n; n--) xchg_spi(0xFF);

  ty = 0;
  if (send_cmd(CMD0, 0) == 1) {
    Timer1 = 1000;
    if (send_cmd(CMD8, 0x1AA) == 1) {
      for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
      if (ocr[2] == 0x01 && ocr[3] == 0xAA) {
        while (Timer1 && send_cmd(ACMD41, 1UL << 30)) {
          vTaskDelay(1 / portTICK_PERIOD_MS);
          if (Timer1 > 0) Timer1--;
        }
        if (Timer1 && send_cmd(CMD58, 0) == 0) {
          for (n = 0; n < 4; n++) ocr[n] = xchg_spi(0xFF);
          ty = (ocr[0] & 0x40) ? CT_SDC2 | CT_BLOCK : CT_SDC2;
        }
      }
    } else {
      if (send_cmd(ACMD41, 0) <= 1) {
        ty = CT_SDC1; cmd = ACMD41;
      } else {
        ty = CT_MMC3; cmd = CMD1;
      }
      while (Timer1 && send_cmd(cmd, 0)) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
        if (Timer1 > 0) Timer1--;
      }
      if (!Timer1 || send_cmd(CMD16, SD_SECTOR_SIZE) != 0)
        ty = 0;
    }
  }
  CardType = ty;
  deselect();

  if (ty) {
    // Switch to fast clock (10MHz for SD card)
    spi_set_speed(10000000);
    Stat &= ~STA_NOINIT;
    ESP_LOGI(TAG, "SD card initialized (SPI mode), type=0x%02X", ty);
  } else {
    ESP_LOGE(TAG, "SD card initialization failed (SPI mode)");
    Stat = STA_NOINIT;
  }

  return Stat;
}

/* SPI mode disk read */
static DRESULT
spi_disk_read(BYTE *buff, LBA_t sector, UINT count)
{
  DWORD sect = (DWORD)sector;

  if (!count) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;

  if (!(CardType & CT_BLOCK)) sect *= 512;

  if (count == 1) {
    if ((send_cmd(CMD17, sect) == 0)
      && rcvr_datablock(buff, 512)) {
      count = 0;
    }
  }
  else {
    if (send_cmd(CMD18, sect) == 0) {
      do {
        if (!rcvr_datablock(buff, 512)) break;
        buff += 512;
      } while (--count);
      send_cmd(CMD12, 0);
    }
  }
  deselect();

  return count ? RES_ERROR : RES_OK;
}

/* SPI mode disk write */
static DRESULT
spi_disk_write(const BYTE *buff, LBA_t sector, UINT count)
{
  DWORD sect = (DWORD)sector;
  if (!count) return RES_PARERR;
  if (Stat & STA_NOINIT) return RES_NOTRDY;
  if (Stat & STA_PROTECT) return RES_WRPRT;

  if (!(CardType & CT_BLOCK)) sect *= 512;

  if (count == 1) {
    if ((send_cmd(CMD24, sect) == 0)
      && xmit_datablock(buff, 0xFE)) {
      count = 0;
    }
  }
  else {
    if (CardType & CT_SDC) send_cmd(ACMD23, count);
    if (send_cmd(CMD25, sect) == 0) {
      do {
        if (!xmit_datablock(buff, 0xFC)) break;
        buff += 512;
      } while (--count);
      if (!xmit_datablock(0, 0xFD)) count = 1;
    }
  }
  deselect();

  return count ? RES_ERROR : RES_OK;
}

/* SPI mode disk ioctl */
static DRESULT
spi_disk_ioctl(BYTE cmd, void *buff)
{
  DRESULT res;
  BYTE n, csd[16];
  DWORD st, ed, csize;
  LBA_t *dp;

  if (Stat & STA_NOINIT) return RES_NOTRDY;

  res = RES_ERROR;

  switch (cmd) {
  case CTRL_SYNC :
    if (select_card()) res = RES_OK;
    break;

  case GET_SECTOR_COUNT :
    if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
      if ((csd[0] >> 6) == 1) {
        csize = csd[9] + ((WORD)csd[8] << 8) + ((DWORD)(csd[7] & 63) << 16) + 1;
        *(LBA_t*)buff = csize << 10;
      } else {
        n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        csize = (csd[8] >> 6) + ((WORD)csd[7] << 2) + ((WORD)(csd[6] & 3) << 10) + 1;
        *(LBA_t*)buff = csize << (n - 9);
      }
      res = RES_OK;
    }
    break;

  case GET_BLOCK_SIZE :
    if (CardType & CT_SDC2) {
      if (send_cmd(ACMD13, 0) == 0) {
        xchg_spi(0xFF);
        if (rcvr_datablock(csd, 16)) {
          for (n = 64 - 16; n; n--) xchg_spi(0xFF);
          *(DWORD*)buff = 16UL << (csd[10] >> 4);
          res = RES_OK;
        }
      }
    } else {
      if ((send_cmd(CMD9, 0) == 0) && rcvr_datablock(csd, 16)) {
        if (CardType & CT_SDC1) {
          *(DWORD*)buff = (((csd[10] & 63) << 1) + ((WORD)(csd[11] & 128) >> 7) + 1) << ((csd[13] >> 6) - 1);
        } else {
          *(DWORD*)buff = ((WORD)((csd[10] & 124) >> 2) + 1) * (((csd[11] & 3) << 3) + ((csd[11] & 224) >> 5) + 1);
        }
        res = RES_OK;
      }
    }
    break;

  case CTRL_TRIM :
    if (!(CardType & CT_SDC)) break;
    if (spi_disk_ioctl(MMC_GET_CSD, csd)) break;
    if (!(csd[10] & 0x40)) break;
    dp = buff; st = (DWORD)dp[0]; ed = (DWORD)dp[1];
    if (!(CardType & CT_BLOCK)) {
      st *= 512; ed *= 512;
    }
    if (send_cmd(CMD32, st) == 0 && send_cmd(CMD33, ed) == 0 && send_cmd(CMD38, 0) == 0 && wait_ready(30000)) {
      res = RES_OK;
    }
    break;

  case MMC_GET_TYPE:
    *(BYTE*)buff = CardType;
    res = RES_OK;
    break;

  case MMC_GET_CSD:
    if (send_cmd(CMD9, 0) == 0 && rcvr_datablock((BYTE*)buff, 16)) {
      res = RES_OK;
    }
    break;

  case MMC_GET_CID:
    if (send_cmd(CMD10, 0) == 0 && rcvr_datablock((BYTE*)buff, 16)) {
      res = RES_OK;
    }
    break;

  case MMC_GET_OCR:
    if (send_cmd(CMD58, 0) == 0) {
      for (n = 0; n < 4; n++) *(((BYTE*)buff) + n) = xchg_spi(0xFF);
      res = RES_OK;
    }
    break;

  case MMC_GET_SDSTAT:
    if (send_cmd(ACMD13, 0) == 0) {
      xchg_spi(0xFF);
      if (rcvr_datablock((BYTE*)buff, 64)) res = RES_OK;
    }
    break;

    case GET_SECTOR_SIZE:
      *((WORD *)buff) = (WORD)SD_SECTOR_SIZE;
      res = RES_OK;
      break;
  default:
    res = RES_PARERR;
  }

  deselect();

  return res;
}


/*=======================================================================*/
/* SDMMC Mode Implementation                                             */
/*=======================================================================*/

static esp_err_t
sdmmc_init(void)
{
  if (sdmmc_initialized) {
    return ESP_OK;
  }

  if (SDMMC_CLK_PIN < 0 || SDMMC_CMD_PIN < 0 || SDMMC_D0_PIN < 0) {
    ESP_LOGE(TAG, "SDMMC pins not configured");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Initializing SDMMC host...");

  // Configure SDMMC host
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_DEFAULT;  // 20 MHz
  host.flags = SDMMC_HOST_FLAG_1BIT;       // 1-bit mode

  // Configure slot with GPIO pins
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = 1;  // 1-bit mode
  slot_config.clk = SDMMC_CLK_PIN;
  slot_config.cmd = SDMMC_CMD_PIN;
  slot_config.d0 = SDMMC_D0_PIN;
  // Internal pull-ups
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  // Allocate card structure
  sdmmc_card = (sdmmc_card_t *)malloc(sizeof(sdmmc_card_t));
  if (sdmmc_card == NULL) {
    ESP_LOGE(TAG, "Failed to allocate card structure");
    return ESP_ERR_NO_MEM;
  }

  // Initialize the SDMMC host
  esp_err_t ret = sdmmc_host_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SDMMC host init failed: %s", esp_err_to_name(ret));
    free(sdmmc_card);
    sdmmc_card = NULL;
    return ret;
  }

  // Initialize the slot
  ret = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SDMMC slot init failed: %s", esp_err_to_name(ret));
    sdmmc_host_deinit();
    free(sdmmc_card);
    sdmmc_card = NULL;
    return ret;
  }

  // Initialize the card
  ret = sdmmc_card_init(&host, sdmmc_card);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SD card init failed: %s", esp_err_to_name(ret));
    sdmmc_host_deinit();
    free(sdmmc_card);
    sdmmc_card = NULL;
    return ret;
  }

  // Print card info
  sdmmc_card_print_info(stdout, sdmmc_card);

  sdmmc_initialized = true;
  ESP_LOGI(TAG, "SDMMC initialized successfully");
  return ESP_OK;
}

static DSTATUS
sdmmc_disk_initialize(void)
{
  ESP_LOGI(TAG, "SDMMC mode SD_disk_initialize called");

  if (sdmmc_init() != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SDMMC");
    return STA_NOINIT;
  }

  Stat &= ~STA_NOINIT;
  return Stat;
}

static DSTATUS
sdmmc_disk_status(void)
{
  if (!sdmmc_initialized || sdmmc_card == NULL) {
    return STA_NOINIT;
  }
  return 0;  // OK
}

static DRESULT
sdmmc_disk_read(BYTE *buff, LBA_t sector, UINT count)
{
  if (!sdmmc_initialized || sdmmc_card == NULL) {
    return RES_NOTRDY;
  }

  esp_err_t ret = sdmmc_read_sectors(sdmmc_card, buff, sector, count);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SDMMC read failed at sector %lu: %s", (unsigned long)sector, esp_err_to_name(ret));
    return RES_ERROR;
  }

  return RES_OK;
}

static DRESULT
sdmmc_disk_write(const BYTE *buff, LBA_t sector, UINT count)
{
  if (!sdmmc_initialized || sdmmc_card == NULL) {
    return RES_NOTRDY;
  }

  esp_err_t ret = sdmmc_write_sectors(sdmmc_card, buff, sector, count);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "SDMMC write failed at sector %lu: %s", (unsigned long)sector, esp_err_to_name(ret));
    return RES_ERROR;
  }

  return RES_OK;
}

static DRESULT
sdmmc_disk_ioctl(BYTE cmd, void *buff)
{
  if (!sdmmc_initialized || sdmmc_card == NULL) {
    return RES_NOTRDY;
  }

  switch (cmd) {
    case CTRL_SYNC:
      return RES_OK;

    case GET_SECTOR_COUNT:
      *(LBA_t *)buff = sdmmc_card->csd.capacity;
      return RES_OK;

    case GET_SECTOR_SIZE:
      *(WORD *)buff = sdmmc_card->csd.sector_size;
      return RES_OK;

    case GET_BLOCK_SIZE:
      *(DWORD *)buff = sdmmc_card->csd.sector_size;
      return RES_OK;

    case CTRL_TRIM:
      return RES_OK;

    default:
      return RES_PARERR;
  }
}


/*=======================================================================*/
/* Public SD_disk_* Functions (dispatch based on mode)                   */
/*=======================================================================*/

int
SD_disk_erase(void)
{
  return 0;
}

DSTATUS
SD_disk_initialize(void)
{
  ESP_LOGI(TAG, "SD_disk_initialize: mode=%d", sd_mode);

  switch (sd_mode) {
    case SD_MODE_SPI:
      return spi_disk_initialize();
    case SD_MODE_SDMMC:
      return sdmmc_disk_initialize();
    default:
      ESP_LOGE(TAG, "SD mode not configured. Call FAT_set_spi_unit or FAT_set_sdmmc_pins first.");
      return STA_NOINIT;
  }
}

DSTATUS
SD_disk_status(void)
{
  switch (sd_mode) {
    case SD_MODE_SPI:
      return Stat;
    case SD_MODE_SDMMC:
      return sdmmc_disk_status();
    default:
      return STA_NOINIT;
  }
}

DRESULT
SD_disk_read(BYTE *buff, LBA_t sector, UINT count)
{
  switch (sd_mode) {
    case SD_MODE_SPI:
      return spi_disk_read(buff, sector, count);
    case SD_MODE_SDMMC:
      return sdmmc_disk_read(buff, sector, count);
    default:
      return RES_NOTRDY;
  }
}

DRESULT
SD_disk_write(const BYTE *buff, LBA_t sector, UINT count)
{
  switch (sd_mode) {
    case SD_MODE_SPI:
      return spi_disk_write(buff, sector, count);
    case SD_MODE_SDMMC:
      return sdmmc_disk_write(buff, sector, count);
    default:
      return RES_NOTRDY;
  }
}

DRESULT
SD_disk_ioctl(BYTE cmd, void *buff)
{
  switch (sd_mode) {
    case SD_MODE_SPI:
      return spi_disk_ioctl(cmd, buff);
    case SD_MODE_SDMMC:
      return sdmmc_disk_ioctl(cmd, buff);
    default:
      return RES_NOTRDY;
  }
}

/*-----------------------------------------------------------------------*/
/* Device timer function                                                 */
/*-----------------------------------------------------------------------*/

void
disk_timerproc(void)
{
  WORD n;
  BYTE s;
  n = Timer1;
  if (n) Timer1 = --n;
  n = Timer2;
  if (n) Timer2 = --n;

  s = Stat;
  Stat = s;
}
