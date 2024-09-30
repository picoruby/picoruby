#include <vm.h>
#include <value.h>

void c_sandbox_state(mrbc_vm *vm, mrbc_value *v, int argc);
void c_sandbox_result(mrbc_vm *vm, mrbc_value *v, int argc);
void c_sandbox_resume(mrbc_vm *vm, mrbc_value *v, int argc);
void c_sandbox_compile(mrbc_vm *vm, mrbc_value *v, int argc);
void c_sandbox_exit(mrbc_vm *vm, mrbc_value *v, int argc);

void create_sandbox(void);

#define SANDBOX_INIT() do { \
  mrbc_class *mrbc_class_Sandbox = mrbc_define_class(vm, "Sandbox", mrbc_class_object); \
  mrbc_define_method(vm, mrbc_class_Sandbox, "compile", c_sandbox_compile); \
  mrbc_define_method(vm, mrbc_class_Sandbox, "resume",  c_sandbox_resume);  \
  mrbc_define_method(vm, mrbc_class_Sandbox, "state",   c_sandbox_state);   \
  mrbc_define_method(vm, mrbc_class_Sandbox, "result",  c_sandbox_result);  \
  mrbc_define_method(vm, mrbc_class_Sandbox, "exit",    c_sandbox_exit);    \
} while (0)


