#include <mrubyc.h>

#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"
#include "../include/sqlite3_vfs.h"

typedef struct {
  sqlite3 *db;
} DbState;

#define D() console_printf("debug: %s\n", __func__)

static void
c_open(mrbc_vm *vm, mrbc_value v[], int argc)
{
  D();
  set_vm_for_vfs(vm);
  sqlite3_os_init();
  /*
   * sqlite3.h only has the statement of sqlite3 struct.
   * Its implementation is in sqlite3.c which is not included here.
   */
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(DbState));
  sqlite3 *db;
  /*
   * pVfs is located at the very top of sqlite3 struct.
   * So this cast works.
   */
  int rc = sqlite3_open_v2(
    (const char *)GET_STRING_ARG(1),
    &db,
    SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,
    VFS_NAME
  );
  DbState *state = (DbState *)self.instance->data;
  state->db = db;
  if (rc != SQLITE_OK) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "sqlite3_open_v2() failed");
    console_printf("\nsqlite3_open_v2() error code: %d\n\n", rc);
    return;
  }
  SET_RETURN(self);
}

static void
c_close(mrbc_vm *vm, mrbc_value v[], int argc)
{
  D();
  DbState *state = (DbState *)v[0].instance->data;
  sqlite3 *db = state->db;
  int rc = sqlite3_close_v2(db);
  if (rc != SQLITE_OK) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "sqlite3_close_v2() failed");
    console_printf("\nsqlite3_close_v2() error code: %d\n\n", rc);
  }
}



/*
 * Usage: SQLite3::Database.vfs_methods = FAT::File.vfs_methods
 */
prb_vfs_methods vfs_methods;
static void
c_vfs_methods_eq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  D();
  mrbc_value methods = GET_ARG(1);
  memcpy(&vfs_methods, methods.instance->data, sizeof(prb_vfs_methods));
}

void
mrbc_sqlite3_class_init(void)
{
  mrbc_class *class_SQLite3 = mrbc_define_class(0, "SQLite3", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(class_SQLite3, mrbc_search_symid("Database"));
  mrbc_class *class_SQLite3_Database = v->cls;

  mrbc_define_method(0, class_SQLite3_Database, "vfs_methods=", c_vfs_methods_eq);

  mrbc_define_method(0, class_SQLite3_Database, "new", c_open);
  mrbc_define_method(0, class_SQLite3_Database, "open", c_open);
  mrbc_define_method(0, class_SQLite3_Database, "close", c_close);
}
