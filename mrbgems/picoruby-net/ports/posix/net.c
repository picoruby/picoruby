#include <unistd.h>


void
Net_sleep_ms(int ms)
{
  usleep(ms * 1000);
}

void
lwip_begin(void)
{
  // no-op
}

void
lwip_end(void)
{
  // no-op
}
