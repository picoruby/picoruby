#include <stdio.h>

#include "../../include/sdmmc.h"

// Note: Actual SDMMC initialization is done in FAT.init_sdmmc()
// This file provides the platform-specific implementation if needed

sdmmc_status_t
SDMMC_init(sdmmc_unit_info_t *unit_info)
{
  // SDMMC initialization is handled by FAT filesystem layer
  // This is a placeholder for potential future use
  return SDMMC_ERROR_NONE;
}
