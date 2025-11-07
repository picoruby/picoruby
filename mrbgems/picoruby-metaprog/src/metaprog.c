#if defined(PICORB_VM_MRUBY)

#error "picoruby-metaprog is not supported. Use mrubyc-metaprog instead."

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/metaprog.c"

#endif

