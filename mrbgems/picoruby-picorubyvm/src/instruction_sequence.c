#include <mruby_compiler.h>
#include "../include/instruction_sequence.h"

typedef struct iseq_binary {
  size_t size;
  uint8_t data[];
} iseq_binary;

#if defined(PICORB_VM_MRUBY)

#include "mruby/instruction_sequence.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/instruction_sequence.c"

#endif
