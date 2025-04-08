#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

void
Net_sleep_ms(int ms)
{
  cyw43_arch_poll();
  sleep_ms(ms);
}

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
