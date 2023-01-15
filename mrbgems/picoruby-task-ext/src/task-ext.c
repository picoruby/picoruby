#include <picorbc.h>
#include <mrubyc.h>

//#ifndef NODE_BOX_SIZE
//#define NODE_BOX_SIZE 20
//#endif
//
//static void
//c_(mrbc_vm *vm, mrbc_value *v, int argc)
//{
//}

void
mrbc_task_ext_init(void)
{
  mrbc_class *class_Task = mrbc_define_class(0, "Task", mrbc_class_object);
//  mrbc_define_method(0, class_Task, "", c_);
//  mrbc_define_method(0, class_Task, "", c_);
//  mrbc_define_method(0, class_Task, "", c_);
}
