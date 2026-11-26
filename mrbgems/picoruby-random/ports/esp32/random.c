#include "esp_random.h"

#include "../../include/picorb_random.h"

/*
 * Takes one byte from each esp_random(). The hardware RNG gives true random
 * numbers only while Wi-Fi or Bluetooth runs. Otherwise ESP-IDF calls its
 * output pseudo-random.
 */
int
picorb_random_bytes(void *buf, size_t len)
{
  unsigned char *p = (unsigned char *)buf;
  size_t i = 0;

  while (i < len) {
    p[i] = (unsigned char)esp_random();
    i++;
  }
  return 0;
}
