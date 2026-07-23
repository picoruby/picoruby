#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/variable.h>

static void
mrb_sqlite3_statement_free(mrb_state *mrb, void *ptr)
{
  DbStatement *cxt = (DbStatement *)ptr;
  if (cxt == NULL) return;
  if (cxt->st) {
    /* Finalizing can roll back, and rolling back reaches the VFS */
    prb_vfs_suspend_calls(true);
    sqlite3_finalize(cxt->st);
    prb_vfs_suspend_calls(false);
    cxt->st = NULL;
  }
  mrb_free(mrb, cxt);
}

const struct mrb_data_type mrb_sqlite3_statement_type = {
  "SQLite3::Statement", mrb_sqlite3_statement_free,
};

static DbStatement *
statement(mrb_state *mrb, mrb_value self)
{
  return (DbStatement *)mrb_data_get_ptr(mrb, self, &mrb_sqlite3_statement_type);
}

static DbStatement *
open_statement(mrb_state *mrb, mrb_value self)
{
  DbStatement *cxt = statement(mrb, self);
  if (cxt->st == NULL) {
    mrb_raise(mrb, mrb_sqlite3_exception_class(mrb), "cannot use a closed statement");
  }
  return cxt;
}

void
prb_sqlite3_raise(mrb_state *mrb, sqlite3 *db, int status)
{
  if ((status & 0xFF) == SQLITE_OK) return;
  const char *err = db ? sqlite3_errmsg(db) : NULL;
  if (err == NULL) err = "SQLite3 error";
  mrb_raise(mrb, mrb_sqlite3_exception_class(mrb), err);
}

static mrb_value
mrb_s_new(mrb_state *mrb, mrb_value klass)
{
  mrb_value connection;
  mrb_value sql;
  mrb_get_args(mrb, "oS", &connection, &sql);

  DbState *state = (DbState *)mrb_data_get_ptr(mrb, connection, &mrb_sqlite3_database_type);
  if (state->closed) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "prepare called on a closed database");
  }

  DbStatement *cxt = (DbStatement *)mrb_malloc(mrb, sizeof(DbStatement));
  cxt->st = NULL;
  cxt->done_p = false;
  mrb_value self = mrb_obj_value(
    Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_sqlite3_statement_type, cxt));

  const char *tail = NULL;
  int rc = sqlite3_prepare_v2(
    state->db,
    RSTRING_PTR(sql),
    (int)RSTRING_LEN(sql),
    &cxt->st,
    &tail
  );
  if (rc != SQLITE_OK) {
    prb_sqlite3_raise(mrb, state->db, rc);
  }

  /* Holding the connection keeps the sqlite3 handle alive while we use it */
  mrb_iv_set(mrb, self, MRB_IVSYM(connection), connection);
  mrb_iv_set(mrb, self, MRB_IVSYM(remainder),
             tail ? mrb_str_new_cstr(mrb, tail) : mrb_str_new_lit(mrb, ""));
  mrb_iv_set(mrb, self, MRB_IVSYM(columns), mrb_nil_value());
  mrb_iv_set(mrb, self, MRB_IVSYM(types), mrb_nil_value());
  return self;
}

static mrb_value
mrb_Statement_close(mrb_state *mrb, mrb_value self)
{
  DbStatement *cxt = statement(mrb, self);
  if (cxt->st) {
    sqlite3_finalize(cxt->st);
    cxt->st = NULL;
  }
  return self;
}

static mrb_value
mrb_Statement_closed_p(mrb_state *mrb, mrb_value self)
{
  return mrb_bool_value(statement(mrb, self)->st == NULL);
}

static mrb_value
mrb_step(mrb_state *mrb, mrb_value self)
{
  DbStatement *cxt = open_statement(mrb, self);
  if (cxt->done_p) return mrb_nil_value();

  int rc = sqlite3_step(cxt->st);
  if (rc == SQLITE_DONE) {
    cxt->done_p = true;
    return mrb_nil_value();
  }
  if (rc != SQLITE_ROW) {
    sqlite3_reset(cxt->st);
    cxt->done_p = false;
    prb_sqlite3_raise(mrb, sqlite3_db_handle(cxt->st), rc);
    return mrb_nil_value();
  }

  int length = sqlite3_column_count(cxt->st);
  mrb_value row = mrb_ary_new_capa(mrb, length);
  int ai = mrb_gc_arena_save(mrb);
  int i = 0;
  while (i < length) {
    mrb_value value;
    switch (sqlite3_column_type(cxt->st, i)) {
      case SQLITE_INTEGER:
        value = mrb_int_value(mrb, (mrb_int)sqlite3_column_int64(cxt->st, i));
        break;
      case SQLITE_FLOAT:
        value = mrb_float_value(mrb, sqlite3_column_double(cxt->st, i));
        break;
      case SQLITE_TEXT:
        value = mrb_str_new(
          mrb,
          (const char *)sqlite3_column_text(cxt->st, i),
          sqlite3_column_bytes(cxt->st, i)
        );
        break;
      case SQLITE_BLOB:
        value = mrb_str_new(
          mrb,
          (const char *)sqlite3_column_blob(cxt->st, i),
          sqlite3_column_bytes(cxt->st, i)
        );
        break;
      default:
        value = mrb_nil_value();
        break;
    }
    mrb_ary_push(mrb, row, value);
    mrb_gc_arena_restore(mrb, ai);
    i++;
  }
  return row;
}

static mrb_value
mrb_reset_bang(mrb_state *mrb, mrb_value self)
{
  DbStatement *cxt = open_statement(mrb, self);
  sqlite3_reset(cxt->st);
  cxt->done_p = false;
  return self;
}

static mrb_value
mrb_done_p(mrb_state *mrb, mrb_value self)
{
  return mrb_bool_value(statement(mrb, self)->done_p);
}

static mrb_value
mrb_column_count(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(sqlite3_column_count(open_statement(mrb, self)->st));
}

/*
 * bind_param(key, value)
 *
 * `key` is either a 1-based parameter index or the parameter's name; a Symbol
 * or String name is looked up the way the sqlite3 gem does.
 */
static mrb_value
mrb_bind_param(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  mrb_value value;
  mrb_get_args(mrb, "oo", &key, &value);
  DbStatement *cxt = open_statement(mrb, self);

  int index;
  if (mrb_integer_p(key)) {
    index = (int)mrb_integer(key);
  } else {
    mrb_value name = mrb_symbol_p(key) ? mrb_sym_str(mrb, mrb_symbol(key)) : key;
    name = mrb_ensure_string_type(mrb, name);
    index = sqlite3_bind_parameter_index(cxt->st, RSTRING_CSTR(mrb, name));
    if (index == 0) {
      /* SQLite parameter names carry their sigil; retry with the usual ':' */
      mrb_value prefixed = mrb_str_new_lit(mrb, ":");
      mrb_str_concat(mrb, prefixed, name);
      index = sqlite3_bind_parameter_index(cxt->st, RSTRING_CSTR(mrb, prefixed));
    }
    if (index == 0) {
      mrb_raisef(mrb, mrb_sqlite3_exception_class(mrb),
                 "no such bind parameter: %v", name);
    }
  }

  int status;
  switch (mrb_type(value)) {
    case MRB_TT_INTEGER:
      status = sqlite3_bind_int64(cxt->st, index, (sqlite3_int64)mrb_integer(value));
      break;
    case MRB_TT_FLOAT:
      status = sqlite3_bind_double(cxt->st, index, (double)mrb_float(value));
      break;
    case MRB_TT_STRING:
      status = sqlite3_bind_text(
        cxt->st,
        index,
        RSTRING_PTR(value),
        (int)RSTRING_LEN(value),
        SQLITE_TRANSIENT
      );
      break;
    case MRB_TT_TRUE:
      status = sqlite3_bind_int64(cxt->st, index, 1);
      break;
    case MRB_TT_FALSE:
      /* MRB_TT_FALSE covers both false and nil */
      status = mrb_nil_p(value)
             ? sqlite3_bind_null(cxt->st, index)
             : sqlite3_bind_int64(cxt->st, index, 0);
      break;
    default:
      mrb_raise(mrb, E_TYPE_ERROR, "cannot bind this type to a statement");
      return self;
  }
  prb_sqlite3_raise(mrb, sqlite3_db_handle(cxt->st), status);
  return self;
}

static mrb_value
mrb_column_name(mrb_state *mrb, mrb_value self)
{
  mrb_int index;
  mrb_get_args(mrb, "i", &index);
  const char *name = sqlite3_column_name(open_statement(mrb, self)->st, (int)index);
  return name ? mrb_str_new_cstr(mrb, name) : mrb_nil_value();
}

static mrb_value
mrb_column_decltype(mrb_state *mrb, mrb_value self)
{
  mrb_int index;
  mrb_get_args(mrb, "i", &index);
  const char *type = sqlite3_column_decltype(open_statement(mrb, self)->st, (int)index);
  return type ? mrb_str_new_cstr(mrb, type) : mrb_nil_value();
}

void
mrb_init_class_SQLite3_Statement(mrb_state *mrb, struct RClass *class_SQLite3)
{
  struct RClass *class_SQLite3_Statement = mrb_define_class_under_id(
    mrb, class_SQLite3, MRB_SYM(Statement), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_SQLite3_Statement, MRB_TT_CDATA);

  mrb_define_class_method_id(mrb, class_SQLite3_Statement, MRB_SYM(new), mrb_s_new, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM(close), mrb_Statement_close, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM_Q(closed), mrb_Statement_closed_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM(step), mrb_step, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM_B(reset), mrb_reset_bang, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM_Q(done), mrb_done_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM(column_count), mrb_column_count, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM(bind_param), mrb_bind_param, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM(column_name), mrb_column_name, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_SQLite3_Statement, MRB_SYM(column_decltype), mrb_column_decltype, MRB_ARGS_REQ(1));
}
