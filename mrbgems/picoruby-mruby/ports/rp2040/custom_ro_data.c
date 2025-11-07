#include <stdbool.h>

extern char __etext; // pico-sdk symbol

bool
mrb_ro_data_p(const char *p)
{
  return p < &__etext;
}
