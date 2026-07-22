#include <string.h>
#include "../include/spi.h"

int
SPI_resolve_unit_num(const char *unit_name, int sck_pin, int cipo_pin, int copi_pin)
{
#if defined(PICORB_PLATFORM_RP2)
  return SPI_unit_num_from_pins(unit_name, sck_pin, cipo_pin, copi_pin);
#else
  (void)sck_pin;
  (void)cipo_pin;
  (void)copi_pin;
  if (strcmp(unit_name, "BITBANG") == 0) {
    return PICORB_SPI_BITBANG;
  }
  return SPI_unit_name_to_unit_num(unit_name);
#endif
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/spi.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/spi.c"

#endif
