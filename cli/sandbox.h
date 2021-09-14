#include "../src/mrubyc/src/vm.h"
#include "../src/mrubyc/src/value.h"

void c_sandbox_state(mrb_vm *vm, mrb_value *v, int argc);
void c_sandbox_result(mrb_vm *vm, mrb_value *v, int argc);
void c_sandbox_resume(mrb_vm *vm, mrb_value *v, int argc);
void c_sandbox_picorbc(mrb_vm *vm, mrb_value *v, int argc);

void create_sandbox(void);

#define SANDBOX_INIT() do { \
  mrbc_define_method(0, mrbc_class_object, "sandbox_picorbc", c_sandbox_picorbc); \
  mrbc_define_method(0, mrbc_class_object, "sandbox_resume",  c_sandbox_resume);  \
  mrbc_define_method(0, mrbc_class_object, "sandbox_state",   c_sandbox_state);   \
  mrbc_define_method(0, mrbc_class_object, "sandbox_result",  c_sandbox_result);  \
} while (0)


