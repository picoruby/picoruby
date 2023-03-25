#include <mrubyc.h>

#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"
#include "../include/sqlite3_vfs.h"

static void
c_open(mrbc_vm *vm, mrbc_value v[], int argc)
{
  /*
   * sqlite3.h only has the statement of sqlite3 struct.
   * Its implementation is in sqlite3.c which is not included here.
   */
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(sqlite3 *));
  sqlite3 *db = (sqlite3 *)self.instance->data;
  int rc = sqlite3_open_v2(
    (const char *)GET_STRING_ARG(1),
    &db,
    SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,
    VFS_NAME
  );
  if (rc != SQLITE_OK) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "sqlite3_open_v2() failed");
    console_printf("\nsqlite3_open_v2() error code: %d\n\n", rc);
    return;
  }
  SET_RETURN(self);
}

/*
 * Usage: SQLite3::Database.vfs_methods = FAT::FILE.vfs_methods
 */
static void
c_vfs_methods_eq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value methods = GET_ARG(1);
  memcpy(&vfs_methods, methods.instance->data, sizeof(prb_vfs_methods));
}

void
mrbc_sqlite3_init(void)
{
  mrbc_class *class_SQLite3 = mrbc_define_class(0, "SQLite3", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(class_SQLite3, mrbc_search_symid("Database"));
  mrbc_class *class_SQLite3_Database = v->cls;

  mrbc_define_method(0, class_SQLite3_Database, "vfs_methods=", c_vfs_methods_eq);

  mrbc_define_method(0, class_SQLite3_Database, "open", c_open);
}
