#include <stdbool.h>
#include <stdint.h>

#include "nrf.h"

#include "../../include/machine.h"

static void
hex32_to_str(uint32_t value, char *dst)
{
  static const char hex[] = "0123456789ABCDEF";

  for (int i = 0; i < 8; i++) {
    dst[7 - i] = hex[value & 0x0f];
    value >>= 4;
  }
}

bool
Machine_get_unique_id(char *id_str)
{
  hex32_to_str(NRF_FICR->DEVICEID[0], &id_str[0]);
  hex32_to_str(NRF_FICR->DEVICEID[1], &id_str[8]);
  id_str[16] = '\0';
  return true;
}
