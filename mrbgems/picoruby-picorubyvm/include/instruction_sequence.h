#if defined(PICORB_VM_MRUBY)

#include <mruby.h>
void mrb_instruction_sequence_init(mrb_state *mrb, struct RClass *class_PicoRubyVM);

#elif defined(PICORB_VM_MRUBYC)

#include <mrubyc.h>
void mrbc_instruction_sequence_init(mrbc_vm *vm, mrbc_class *class_PicoRubyVM);

#endif

