#ifndef SPI_DEFINED_H_
#define SPI_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICORUBY_SPI_BITBANG          0
#define PICORUBY_SPI_RP2040_SPI0      1
#define PICORUBY_SPI_RP2040_SPI1      2

#define UNIT_SELECT() \
  do { \
    switch (unit_info->unit_num) { \
      case PICORUBY_SPI_BITBANG:     { unit = NULL; break; } \
      case PICORUBY_SPI_RP2040_SPI0: { unit = spi0; break; } \
      case PICORUBY_SPI_RP2040_SPI1: { unit = spi1; break; } \
      default: { return ERROR_INVALID_UNIT; } \
    } \
  } while (0)

typedef enum {
  ERROR_NONE              =  0,
  ERROR_INVALID_UNIT      = -1,
  ERROR_INVALID_MODE      = -2,
  ERROR_INVALID_FIRST_BIT = -3,
  ERROR_NOT_IMPLEMENTED   = -4,
} spi_status_t;

typedef struct spi_unit_info {
  uint8_t  unit_num;
  int8_t   sck_pin;
  int8_t   copi_pin;
  int8_t   cipo_pin;
  int8_t   cs_pin;
  uint32_t frequency;
  uint8_t  mode;
  uint8_t  first_bit;
  uint8_t  data_bits;
} spi_unit_info_t;

int SPI_unit_name_to_unit_num(const char *);
spi_status_t SPI_gpio_init(spi_unit_info_t*);
int SPI_read_blocking(spi_unit_info_t*, uint8_t*, size_t, uint8_t);
int SPI_write_blocking(spi_unit_info_t*, uint8_t*, size_t);
int SPI_transfer(spi_unit_info_t*, uint8_t*, uint8_t*, size_t);

#ifdef __cplusplus
}
#endif

#endif /* SPI_DEFINED_H_ */

