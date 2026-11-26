#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/debug.h>
#include <mruby/error.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/variable.h>
#include <mruby/presym.h>
#include "version.h"
#include <string.h>

extern const char *prebuilt_gems[];

static mrb_value
mrb_extern(mrb_state *mrb, mrb_value self)
{
  char *name;
  mrb_bool force;
  mrb_get_args(mrb, "z|b", &name, &force);
  for (int i = 0; prebuilt_gems[i]; i++) {
    if (strcmp(prebuilt_gems[i], name) == 0) {
      return mrb_false_value();
    }
  }
  return mrb_nil_value();
}

/* The file of the code that called the C method running now: the nearest
   Ruby frame below it. NULL when that code has no file, as for an eval
   given none ("(eval)" in both picoruby-eval and mruby-eval). */
static const char *
caller_filename(mrb_state *mrb)
{
  mrb_callinfo *ci = mrb->c->ci;
  while (mrb->c->cibase < ci) {
    ci--;
    const struct RProc *proc = ci->proc;
    if (!proc || MRB_PROC_CFUNC_P(proc)) continue;
    const mrb_irep *irep = proc->body.irep;
    if (!irep || !ci->pc) return NULL;
    const char *file = mrb_debug_get_filename(mrb, irep, (uint32_t)(ci->pc - irep->iseq - 1));
    if (file && strcmp(file, "(eval)") == 0) return NULL;
    return file;
  }
  return NULL;
}

/* The directory of the caller's file, as CRuby has it: the real path of a
   file that exists, else the name as given (an eval given a file name).
   nil when the caller has no file. The File of picoruby-vfs has no
   realpath; the VFS has no symbolic links, so the name as given is real. */
static mrb_value
caller_dir(mrb_state *mrb)
{
  const char *file = caller_filename(mrb);
  if (!file) return mrb_nil_value();
  mrb_value file_obj = mrb_obj_value(mrb_class_get_id(mrb, MRB_SYM(File)));
  mrb_value path = mrb_str_new_cstr(mrb, file);
  if (mrb_respond_to(mrb, file_obj, MRB_SYM(realpath)) &&
      mrb_test(mrb_funcall_id(mrb, file_obj, MRB_SYM_Q(exist), 1, path))) {
    path = mrb_funcall_id(mrb, file_obj, MRB_SYM(realpath), 1, path);
  }
  return mrb_funcall_id(mrb, file_obj, MRB_SYM(dirname), 1, path);
}

static mrb_value
mrb_f_dir(mrb_state *mrb, mrb_value self)
{
  return caller_dir(mrb);
}

static mrb_value
mrb_f_require_relative(mrb_state *mrb, mrb_value self)
{
  mrb_value path;
  mrb_get_args(mrb, "S", &path);
  mrb_value dir = caller_dir(mrb);
  if (mrb_nil_p(dir)) {
    mrb_raise(mrb, mrb_class_get_id(mrb, MRB_SYM(LoadError)), "cannot infer basepath");
  }
  struct RClass *file_class = mrb_class_get_id(mrb, MRB_SYM(File));
  path = mrb_funcall_id(mrb, mrb_obj_value(file_class), MRB_SYM(expand_path), 2, path, dir);
  return mrb_funcall_id(mrb, self, MRB_SYM(require), 1, path);
}

void
mrb_picoruby_require_gem_init(mrb_state* mrb)
{
  char platform[128];
  char description[256];
  Platform_name(platform, sizeof(platform));
  picorb_description((const char *)platform, description, sizeof(description));
  // RUBY_VERSION is defined in mruby
  mrb_define_global_const(mrb, "RITE_VERSION", mrb_str_new_static(mrb, picorb_rite_version(), strlen(picorb_rite_version())));
  mrb_define_global_const(mrb, "PICORUBY_VERSION", mrb_str_new_static(mrb, picorb_version(), strlen(picorb_version())));
  mrb_define_global_const(mrb, "RUBY_PLATFORM", mrb_str_new_cstr(mrb, platform));
  mrb_define_global_const(mrb, "RUBY_DESCRIPTION", mrb_str_new_cstr(mrb, description));

  struct RClass *module_Kernel = mrb_define_module_id(mrb, MRB_SYM(Kernel));

  mrb_define_private_method_id(mrb, module_Kernel, MRB_SYM(extern), mrb_extern, MRB_ARGS_ARG(1,1));
  mrb_define_private_method_id(mrb, module_Kernel, MRB_SYM(__dir__), mrb_f_dir, MRB_ARGS_NONE());
  mrb_define_private_method_id(mrb, module_Kernel, MRB_SYM(require_relative), mrb_f_require_relative, MRB_ARGS_REQ(1));

  mrb_value loaded_features = mrb_ary_new(mrb);
  mrb_gv_set(mrb, MRB_GVSYM(LOADED_FEATURES), loaded_features);
  for (int i = 0; prebuilt_gems[i]; i++) {
    mrb_ary_push(mrb, loaded_features, mrb_str_new_cstr(mrb, prebuilt_gems[i]));
  }
}

void
mrb_picoruby_require_gem_final(mrb_state* mrb)
{
}
