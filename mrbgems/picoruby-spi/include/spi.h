#ifndef SPI_DEFINED_H_
#define SPI_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICORB_SPI_BITBANG          0
#define PICORB_SPI_RP2040_SPI0      1
#define PICORB_SPI_RP2040_SPI1      2

#define UNIT_SELECT() \
  do { \
    switch (unit_info->unit_num) { \
      case PICORB_SPI_BITBANG:     { unit = NULL; break; } \
      case PICORB_SPI_RP2040_SPI0: { unit = spi0; break; } \
      case PICORB_SPI_RP2040_SPI1: { unit = spi1; break; } \
      default: { return SPI_ERROR_INVALID_UNIT; } \
    } \
  } while (0)

typedef enum {
  SPI_ERROR_NONE              =  0,
  SPI_ERROR_INVALID_UNIT      = -1,
  SPI_ERROR_INVALID_MODE      = -2,
  SPI_ERROR_INVALID_FIRST_BIT = -3,
  SPI_ERROR_NOT_IMPLEMENTED   = -4,
  SPI_ERROR_FAILED_TO_INIT    = -5,
  SPI_ERROR_FAILED_TO_ADD_DEV = -6,
  SPI_ERROR_UNIT_MISMATCH     = -7,
  SPI_ERROR_UNDETERMINED      = -8,
} spi_status_t;

typedef struct spi_unit_info {
  uint32_t frequency;
  uint8_t  unit_num;
  int8_t   sck_pin;
  int8_t   copi_pin;
  int8_t   cipo_pin;
  int8_t   cs_pin;
  uint8_t  mode;
  uint8_t  first_bit;
  uint8_t  data_bits;
} spi_unit_info_t;

#define MAX_STACK_BUFFER_SIZE 256

int SPI_unit_name_to_unit_num(const char *);
/*
 * Resolve the unit number from the unit name and the SCK/CIPO/COPI pins.
 * On RP2 the unit can be inferred from the pins when the name is empty, and a
 * mismatch between name and pins (or between pins) yields a negative error code.
 * The CS pin is a plain GPIO and is not considered. On other platforms this
 * falls back to the name-based lookup (with the BITBANG special case preserved).
 */
int SPI_resolve_unit_num(const char *unit_name, int sck_pin, int cipo_pin, int copi_pin);
int SPI_unit_num_from_pins(const char *unit_name, int sck_pin, int cipo_pin, int copi_pin);
spi_status_t SPI_gpio_init(spi_unit_info_t*);
int SPI_read_blocking(spi_unit_info_t*, uint8_t*, size_t, uint8_t);
int SPI_write_blocking(spi_unit_info_t*, uint8_t*, size_t);
int SPI_transfer(spi_unit_info_t*, uint8_t*, uint8_t*, size_t);

#ifdef __cplusplus
}
#endif

#endif /* SPI_DEFINED_H_ */

