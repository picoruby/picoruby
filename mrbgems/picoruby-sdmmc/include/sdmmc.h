#ifndef SDMMC_DEFINED_H_
#define SDMMC_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SDMMC_ERROR_NONE              =  0,
  SDMMC_ERROR_INVALID_PIN       = -1,
  SDMMC_ERROR_INVALID_WIDTH     = -2,
  SDMMC_ERROR_NOT_IMPLEMENTED   = -3,
  SDMMC_ERROR_FAILED_TO_INIT    = -4,
} sdmmc_status_t;

typedef struct sdmmc_unit_info {
  int8_t   clk_pin;
  int8_t   cmd_pin;
  int8_t   d0_pin;
  int8_t   d1_pin;
  int8_t   d2_pin;
  int8_t   d3_pin;
  uint8_t  width;   // 1 or 4 bit mode
} sdmmc_unit_info_t;

sdmmc_status_t SDMMC_init(sdmmc_unit_info_t *unit_info);

#ifdef __cplusplus
}
#endif

#endif /* SDMMC_DEFINED_H_ */
