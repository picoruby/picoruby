#include <string.h>
#include "../include/i2c.h"

int
I2C_resolve_unit_num(const char *unit_name, int sda_pin, int scl_pin)
{
#if defined(PICORB_PLATFORM_RP2)
  return I2C_unit_num_from_pins(unit_name, sda_pin, scl_pin);
#else
  (void)sda_pin;
  (void)scl_pin;
  return I2C_unit_name_to_unit_num(unit_name);
#endif
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/i2c.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/i2c.c"

#endif
