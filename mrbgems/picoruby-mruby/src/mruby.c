#include "mruby.h"

#if defined(PICORB_PLATFORM_POSIX)
void mrb_file_ext_init(mrb_state *mrb);
#endif

void
mrb_picoruby_mruby_gem_init(mrb_state* mrb)
{
#if defined(PICORB_PLATFORM_POSIX)
  mrb_file_ext_init(mrb);
#endif
}

void
mrb_picoruby_mruby_gem_final(mrb_state* mrb)
{
}
