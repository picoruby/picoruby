#include <mruby.h>
#include <mruby/hash.h>
#include <mruby/string.h>
#include <mruby/presym.h>

static mrb_value
mrb_next_executable(mrb_state *mrb, mrb_value self)
{
  static int i = 0;
  if (executables[i].path) {
    const uint8_t *vm_code = executables[i].vm_code;
    mrb_value hash = mrb_hash_new_capa(mrb, 3);
    mrb_value path = mrb_str_new_cstr(mrb, (char *)executables[i].path);
    mrb_hash_set(mrb, hash,
                mrb_symbol_value(MRB_SYM(path)),
                mrb_str_dup(mrb, path));
    uint32_t codesize = (vm_code[8] << 24) + (vm_code[9] << 16) + (vm_code[10] << 8) + vm_code[11];
    mrb_value code_val = mrb_str_new(mrb, (char *)vm_code, codesize);
    mrb_hash_set(mrb, hash,
                mrb_symbol_value(MRB_SYM(code)),
                mrb_str_dup(mrb, code_val));
    mrb_hash_set(mrb, hash,
                mrb_symbol_value(MRB_SYM(crc)),
                mrb_int_value(mrb, executables[i].crc));
    i++;
    return hash;
  } else {
    return mrb_nil_value();
  }
}

void
mrb_picoruby_shell_gem_init(mrb_state *mrb)
{
  struct RClass *class_Shell = mrb_define_class_id(mrb, MRB_SYM(Shell), mrb->object_class);
  mrb_define_class_method_id(mrb, class_Shell, MRB_SYM(next_executable), mrb_next_executable, MRB_ARGS_NONE());
}

void
mrb_picoruby_shell_gem_final(mrb_state* mrb)
{
}
