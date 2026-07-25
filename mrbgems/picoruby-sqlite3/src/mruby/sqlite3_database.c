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

static DbState *
open_db_state(mrb_state *mrb, mrb_value self)
{
  DbState *state = db_state(mrb, self);
  if (state->closed || state->db == NULL) {
    mrb_raise(mrb, mrb_sqlite3_exception_class(mrb), "cannot use a closed database");
  }
  return state;
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

static mrb_value
mrb_Database_last_insert_row_id(mrb_state *mrb, mrb_value self)
{
  sqlite3_int64 id = sqlite3_last_insert_rowid(open_db_state(mrb, self)->db);
  /* rowid is 64-bit; these builds define MRB_INT64, so mrb_int_value keeps it */
  return mrb_int_value(mrb, (mrb_int)id);
}

static mrb_value
mrb_Database_changes(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(sqlite3_changes(open_db_state(mrb, self)->db));
}

static mrb_value
mrb_Database_total_changes(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(sqlite3_total_changes(open_db_state(mrb, self)->db));
}

static mrb_value
mrb_Database_transaction_active_p(mrb_state *mrb, mrb_value self)
{
  /* Outside a transaction SQLite is in autocommit mode */
  return mrb_bool_value(sqlite3_get_autocommit(open_db_state(mrb, self)->db) == 0);
}

static mrb_value
mrb_Database_readonly_p(mrb_state *mrb, mrb_value self)
{
  /* sqlite3_db_readonly returns 1 (read-only), 0 (read-write) or -1 (no such
     database name); treat the -1 case as false */
  return mrb_bool_value(sqlite3_db_readonly(open_db_state(mrb, self)->db, "main") == 1);
}

static mrb_value
mrb_Database_filename(mrb_state *mrb, mrb_value self)
{
  const char *name = sqlite3_db_filename(open_db_state(mrb, self)->db, "main");
  /* A temporary or in-memory database reports an empty name */
  return (name && name[0]) ? mrb_str_new_cstr(mrb, name) : mrb_nil_value();
}

/*
 * execute_batch(sql) -> nil
 *
 * Runs a script of one or more semicolon separated statements, discarding any
 * rows they produce. Handy for schema setup and migrations. Parameter binding
 * is not supported here (use #execute for that).
 */
static mrb_value
mrb_Database_execute_batch(mrb_state *mrb, mrb_value self)
{
  const char *sql;
  mrb_get_args(mrb, "z", &sql);
  sqlite3 *db = open_db_state(mrb, self)->db;

  char *errmsg = NULL;
  int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
  if (rc != SQLITE_OK) {
    /* sqlite3_exec owns errmsg; copy it into an exception before freeing */
    mrb_value message = errmsg ? mrb_str_new_cstr(mrb, errmsg)
                               : mrb_str_new_lit(mrb, "SQLite3 error");
    if (errmsg) sqlite3_free(errmsg);
    mrb_raise(mrb, mrb_sqlite3_exception_class_for(mrb, rc), RSTRING_PTR(message));
  }
  return mrb_nil_value();
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
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM(last_insert_row_id), mrb_Database_last_insert_row_id, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM(changes), mrb_Database_changes, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM(total_changes), mrb_Database_total_changes, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM_Q(transaction_active), mrb_Database_transaction_active_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM(execute_batch), mrb_Database_execute_batch, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM_Q(readonly), mrb_Database_readonly_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Database, MRB_SYM(filename), mrb_Database_filename, MRB_ARGS_NONE());
}
