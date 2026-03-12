#include <stdint.h>
#include <stddef.h>

static uint32_t
generate_crc32(uint8_t *str, size_t len, uint32_t crc)
{
  uint32_t crc_value = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc_value ^= str[i];
    for (int j = 0; j < 8; j++) {
      if (crc_value & 1) {
        crc_value = (crc_value >> 1) ^ 0xEDB88320;
      } else {
        crc_value >>= 1;
      }
    }
  }
  return ~crc_value;
}

static uint16_t
generate_crc16(uint8_t *str, size_t len, uint16_t crc)
{
  for (size_t i = 0; i < len; i++) {
    crc ^= (uint16_t)str[i] << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc <<= 1;
      }
    }
  }
  return crc & 0xFFFF;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/crc.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/crc.c"

#endif

