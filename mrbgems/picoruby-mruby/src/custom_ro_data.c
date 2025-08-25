#include "mruby.h"
#include "../include/custom_ro_data.h"

#if defined(MRB_USE_CUSTOM_RO_DATA_P)
mrb_bool
mrb_ro_data_p(const char *p)
{
  return PORT_mrb_ro_data_p(p) ? TRUE : FALSE;
}
#endif
