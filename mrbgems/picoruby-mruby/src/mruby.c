#include "mruby.h"

void mrb_file_ext_init(mrb_state *mrb);

void
mrb_picoruby_mruby_gem_init(mrb_state* mrb)
{
  mrb_file_ext_init(mrb);
}

void
mrb_picoruby_mruby_gem_final(mrb_state* mrb)
{
}
