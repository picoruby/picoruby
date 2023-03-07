#include <mrubyc.h>

void
mrbc_task_ext_init(void)
{
  mrbc_class *class_Task = mrbc_define_class(0, "Task", mrbc_class_object);
}
