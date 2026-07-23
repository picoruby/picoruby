#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/presym.h>
#include <mruby/string.h>

static void
mrb_sqlite3_database_free(mrb_state *mrb, void *ptr)
{
  DbState *state = (DbState *)ptr;
  if (state == NULL) return;
  if (state->db) {
    /* A database that was never closed is torn down without the VM */
    prb_vfs_suspend_calls(true);
    sqlite3_close_v2(state->db);
    prb_vfs_suspend_calls(false);
    state->db = NULL;
  }
  mrb_free(mrb, state);
}

const struct mrb_data_type mrb_sqlite3_database_type = {
  "SQLite3::Database", mrb_sqlite3_database_free,
};

static DbState *
db_state(mrb_state *mrb, mrb_value self)
{
  return (DbState *)mrb_data_get_ptr(mrb, self, &mrb_sqlite3_database_type);
}

/*
 * SQLite3::Database._open(driver, path)
 *
 * `driver` is the picoruby-vfs driver that owns `path` (a Littlefs instance,
 * for example). `path` is relative to that driver's mountpoint; the driver
 * prepends its own prefix, so every path SQLite derives from it (journals,
 * temporary files) stays valid.
 */
static mrb_value
mrb_s__open(mrb_state *mrb, mrb_value klass)
{
  mrb_value driver;
  const char *path;
  mrb_get_args(mrb, "oz", &driver, &path);

  prb_vfs_set_driver(mrb, driver);
  sqlite3_os_init();

  DbState *state = (DbState *)mrb_malloc(mrb, sizeof(DbState));
  state->db = NULL;
  state->closed = false;
  mrb_value self = mrb_obj_value(
    Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_sqlite3_database_type, state));

  sqlite3 *db = NULL;
  int rc = sqlite3_open_v2(
    path,
    &db,
    SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,
    VFS_NAME
  );
  /* Assign before checking rc: sqlite3_open_v2() may hand back a handle that
     still has to be closed even when it failed */
  state->db = db;
  if (rc != SQLITE_OK) {
    prb_sqlite3_raise(mrb, db, rc);
  }
  return self;
}

static mrb_value
mrb_Database_close(mrb_state *mrb, mrb_value self)
{
  DbState *state = db_state(mrb, self);
  if (state->closed) return mrb_nil_value();

  int rc = sqlite3_close_v2(state->db);
  /* close anyway */
  state->db = NULL;
  state->closed = true;
  if (rc != SQLITE_OK) {
    mrb_raisef(mrb, mrb_sqlite3_exception_class(mrb),
               "sqlite3_close_v2() failed (%d)", rc);
  }
  return mrb_nil_value();
}

static mrb_value
mrb_Database_closed_p(mrb_state *mrb, mrb_value self)
{
  return mrb_bool_value(db_state(mrb, self)->closed);
}

void
mrb_init_class_SQLite3_Database(mrb_state *mrb, struct RClass *class_SQLite3)
{
  struct RClass *class_SQLite3_Database = mrb_define_class_under_id(
    mrb, class_SQLite3, MRB_SYM(Database), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_SQLite3_Database, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_SQLite3_Database, MRB_SYM(_open), mrb_s__open, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM(close), mrb_Database_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM_Q(closed), mrb_Database_closed_p, MRB_ARGS_NONE());
}
