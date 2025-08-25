#ifndef MRUBY_CUSTOM_RO_DATA_H
#define MRUBY_CUSTOM_RO_DATA_H

#include <picoruby.h>
#include <stdbool.h>

MRB_BEGIN_DECL

bool PORT_mrb_ro_data_p(const char *p);

MRB_END_DECL

#endif
