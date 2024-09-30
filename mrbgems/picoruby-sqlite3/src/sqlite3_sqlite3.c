#include "../include/sqlite3.h"

/*
 * Usage:
 *  SQLite3.vfs_methods = FAT.vfs_methods
 *  SQLite3.time_methods = Time.time_methods
 */
prb_vfs_methods vfs_methods = {0};
prb_time_methods time_methods = {0};

static void
c_vfs_methods_eq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value methods = GET_ARG(1);
  if (vfs_methods.file_new != NULL &&
      memcmp(&vfs_methods, methods.instance->data, sizeof(prb_vfs_methods)) != 0)
  {
    console_printf("WARN: changing vfs_methods may break existing database connection\n");
  }
  memcpy(&vfs_methods, methods.instance->data, sizeof(prb_vfs_methods));
}

static void
c_time_methods_eq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value methods = GET_ARG(1);
  memcpy(&time_methods, methods.instance->data, sizeof(prb_time_methods));
}

void
mrbc_sqlite3_init(mrbc_vm *vm)
{
  mrbc_class *class_SQLite3 = mrbc_define_class(vm, "SQLite3", mrbc_class_object);
  mrbc_define_method(vm, class_SQLite3, "vfs_methods=", c_vfs_methods_eq);
  mrbc_define_method(vm, class_SQLite3, "time_methods=", c_time_methods_eq);

  mrbc_init_class_SQLite3_Database(vm, class_SQLite3);
  mrbc_init_class_SQLite3_Statement(vm, class_SQLite3);
}

