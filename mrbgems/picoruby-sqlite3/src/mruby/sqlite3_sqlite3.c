#include <mruby/class.h>
#include <mruby/presym.h>

struct RClass *
mrb_sqlite3_exception_class(mrb_state *mrb)
{
  struct RClass *class_SQLite3 = mrb_class_get_id(mrb, MRB_SYM(SQLite3));
  return mrb_class_get_under_id(mrb, class_SQLite3, MRB_SYM(Exception));
}

void
mrb_picoruby_sqlite3_gem_init(mrb_state *mrb)
{
  struct RClass *class_SQLite3 = mrb_define_class_id(mrb, MRB_SYM(SQLite3), mrb->object_class);

  /* Defined here rather than in mrblib so that the C code can always raise it */
  mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(Exception), mrb->eStandardError_class);

  mrb_init_class_SQLite3_Database(mrb, class_SQLite3);
  mrb_init_class_SQLite3_Statement(mrb, class_SQLite3);
}

void
mrb_picoruby_sqlite3_gem_final(mrb_state *mrb)
{
  prb_vfs_forget_driver(mrb);
}
