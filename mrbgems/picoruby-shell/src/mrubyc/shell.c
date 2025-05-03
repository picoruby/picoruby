#include "mrubyc.h"

static void
c_exit(mrbc_vm *vm, mrbc_value *v, int argc)
{
  pid_t pid = getpid();
  kill(pid, SIGINT);
}

static void
c_next_executable(mrbc_vm *vm, mrbc_value *v, int argc)
{
  static int i = 0;
  if (executables[i].path) {
    const uint8_t *vm_code = executables[i].vm_code;
    mrbc_value hash = mrbc_hash_new(vm, 3);
    mrbc_value path = mrbc_string_new_cstr(vm, (char *)executables[i].path);
    mrbc_hash_set(&hash,
      &mrbc_symbol_value(mrbc_str_to_symid("path")),
      &path
    );
    uint32_t codesize = (vm_code[8] << 24) + (vm_code[9] << 16) + (vm_code[10] << 8) + vm_code[11];
    mrbc_value code_val = mrbc_string_new(vm, vm_code, codesize);
    mrbc_hash_set(&hash,
      &mrbc_symbol_value(mrbc_str_to_symid("code")),
      &code_val
    );
    mrbc_hash_set(&hash,
      &mrbc_symbol_value(mrbc_str_to_symid("crc")),
      &mrbc_integer_value(executables[i].crc)
    );
    SET_RETURN(hash);
    i++;
  } else {
    SET_NIL_RETURN();
  }
}

void
mrbc_shell_init(mrbc_vm *vm)
{
  mrbc_define_method(vm, mrbc_class_object, "exit", c_exit);

  mrbc_class *mrbc_class_Shell = mrbc_define_class(vm, "Shell", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_Shell, "next_executable", c_next_executable);
}
