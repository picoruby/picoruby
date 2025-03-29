#include "pico/cyw43_arch.h"

void
lwip_begin(void)
{
  cyw43_arch_lwip_begin();
}

void
lwip_end(void)
{
  cyw43_arch_lwip_end();
}
