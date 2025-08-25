#include "../../include/custom_ro_data.h"

extern char __etext; // pico-sdk symbol

bool
PORT_mrb_ro_data_p(const char *p)
{
  return p < &__etext;
}
