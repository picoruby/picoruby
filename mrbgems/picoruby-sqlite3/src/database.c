#include <mrubyc.h>

#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"
#include "../include/sqlite3_vfs.h"

typedef struct {
  sqlite3 *db;
} DbState;

//#define D() console_printf("debug: %s\n", __func__)
#define D() (void)0

/*
 * Usage: SQLite3::Database.vfs_methods = FAT::File.vfs_methods
 */
prb_vfs_methods vfs_methods = {0};
static void
c_vfs_methods_eq(mrbc_vm *vm, mrbc_value v[], int argc)
{
  D();
  mrbc_value methods = GET_ARG(1);
  if (vfs_methods.file_new != NULL &&
      memcmp(&vfs_methods, methods.instance->data, sizeof(prb_vfs_methods)) != 0)
  {
    console_printf("WARN: changing vfs_methods may break existing database connection\n");
  }
  memcpy(&vfs_methods, methods.instance->data, sizeof(prb_vfs_methods));
}


static void
c__open(mrbc_vm *vm, mrbc_value v[], int argc)
{
  D();
  if (vfs_methods.file_new == NULL) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "vfs_methods is not set");
    return;
  };
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

static void
array_callback_funciton(mrbc_value *result_array, int count, char **data, char **columns)
{
  D();
  mrbc_vm *vm = get_vm_for_vfs();
  mrbc_value row = mrbc_array_new(vm, count);
  for (int i = 0; i < count; i++) {
    mrbc_value val;
    if (data[i] == NULL) {
      val = mrbc_nil_value();
    } else {
      val = mrbc_string_new_cstr(vm, data[i]);
    }
    mrbc_array_push(&row, &val);
  }
  mrbc_array_push(result_array, &row);
}

static void
c__execute(mrbc_vm *vm, mrbc_value v[], int argc)
{
  D();
  DbState *state = (DbState *)v[0].instance->data;
  sqlite3 *db = state->db;
  char *err_msg = NULL;
  mrbc_value result_array = mrbc_array_new(vm, 0);
  int rc = sqlite3_exec(
              db,
              (const char *)GET_STRING_ARG(1),
              (sqlite3_callback)array_callback_funciton,
              &result_array,
              &err_msg
            );
  if (rc != SQLITE_OK) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "sqlite3_exec() failed");
    console_printf("code: %d, error message: %s\n", rc, err_msg);
    mrbc_raw_free(err_msg);
  }
  SET_RETURN(result_array);
}

void
mrbc_sqlite3_init(void)
{
  mrbc_class *class_SQLite3 = mrbc_define_class(0, "SQLite3", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(class_SQLite3, mrbc_search_symid("Database"));
  mrbc_class *class_SQLite3_Database = v->cls;

  mrbc_define_method(0, class_SQLite3, "vfs_methods=", c_vfs_methods_eq);

  mrbc_define_method(0, class_SQLite3_Database, "close", c_close);
  mrbc_define_method(0, class_SQLite3_Database, "_open", c__open);
  mrbc_define_method(0, class_SQLite3_Database, "_execute", c__execute);
}
