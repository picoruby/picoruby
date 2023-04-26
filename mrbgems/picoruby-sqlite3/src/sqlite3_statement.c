#include "../include/sqlite3.h"

typedef struct {
  sqlite3_stmt *st;
  int done_p;
} DbStatement;

#define GETIV(str)      mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define SETIV(str, val) mrbc_instance_setiv(&v[0], mrbc_str_to_symid(#str), val)

void prb_sqlite3_raise(mrbc_vm *vm, sqlite3 *db, int status)
{
  switch(status & 0xFF) {
    case SQLITE_OK:
      return;
      break;
    default:
      break;
  }
  const char *err = sqlite3_errmsg(db);
  mrbc_raise(vm, MRBC_CLASS(RuntimeError), err);
}

static void
c_Statement_new(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DbState *state = (DbState *)v[1].instance->data;
  if (state->closed) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "prepare called on a closed database");
    return;
  }
  if (v[2].tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
    return;
  }
  mrbc_value self = mrbc_instance_new(vm, v->cls, sizeof(DbStatement));
  DbStatement *cxt = (DbStatement *)self.instance->data;
  cxt->done_p = 0;
  const char *tail = NULL;
  int rc = sqlite3_prepare_v2(
    state->db,
    (const char *)v[2].string->data,
    (int)v[2].string->size,
    &cxt->st,
    &tail
  );
  if (rc != SQLITE_OK) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "sqlite3_prepare_v2() failed");
    console_printf("\nsqlite3_prepare_v2() error code: %d\n\n", rc);
    return;
  }
  SET_RETURN(self);
  SETIV(connection, &v[1]);
  mrbc_value remainder = mrbc_string_new_cstr(vm, tail);
  SETIV(remainder, &remainder);
  SETIV(columns, &mrbc_nil_value());
  SETIV(types, &mrbc_nil_value());
//  mrbc_incref(&self);
}

static void
c_Statement_close(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DbStatement *cxt = (DbStatement *)v[0].instance->data;
  if (cxt->st) {
    sqlite3_finalize(cxt->st);
    cxt->st = NULL;
  }
  SET_RETURN(v[0]);
  v[0].obj->ref_count = 0;
}

static void
c_Statement_closed_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DbStatement *cxt = (DbStatement *)v[0].instance->data;
  if (cxt->st) {
    SET_FALSE_RETURN();
  } else {
    SET_TRUE_RETURN();
  }
}

static void
c_Statement_step(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DbStatement *cxt = (DbStatement *)v[0].instance->data;
  sqlite3_stmt *stmt = cxt->st;
  if (cxt->done_p) {
    SET_NIL_RETURN();
    return;
  }
  int value = sqlite3_step(stmt);
  int length = sqlite3_column_count(stmt);
  mrbc_value list = mrbc_array_new(vm, length);

  switch (value) {
    case SQLITE_ROW:
      {
        int i;
        for (i = 0; i < length; i++) {
          mrbc_value value;
          switch (sqlite3_column_type(stmt, i)) {
            case SQLITE_INTEGER:
              value = mrbc_integer_value(sqlite3_column_int64(stmt, i));
              break;
            case SQLITE_FLOAT:
              value = mrbc_float_value(vm, sqlite3_column_double(stmt, i));
              break;
            case SQLITE_TEXT:
              value = mrbc_string_new_cstr(vm, (const char *)sqlite3_column_text(stmt, i));
              break;
            case SQLITE_BLOB:
              value = mrbc_string_new(
                vm,
                (const char *)sqlite3_column_blob(stmt, i),
                sqlite3_column_bytes(stmt, i)
              );
              break;
            case SQLITE_NULL:
              value = mrbc_nil_value();
              break;
            default:
              value = mrbc_nil_value();
              mrbc_raise(vm, MRBC_CLASS(RuntimeError), "bad type");
          }
          mrbc_array_push(&list, &value);
        }
      }
      break;
    case SQLITE_DONE:
      cxt->done_p = 1;
      SET_NIL_RETURN();
      break;
    default:
      sqlite3_reset(stmt);
      cxt->done_p = 0;
      prb_sqlite3_raise(vm, sqlite3_db_handle(cxt->st), value);
  }
  SET_RETURN(list);
}

static void
c_Statement_reset_bang(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DbStatement *cxt = (DbStatement *)v[0].instance->data;
  sqlite3_reset(cxt->st);
  cxt->done_p = 0;
  SET_RETURN(v[0]);
  mrbc_incref(&v[0]);
}

static void
c_Statement_done_q(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DbStatement *cxt = (DbStatement *)v[0].instance->data;
  if (cxt->done_p) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_Statement_column_count(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DbStatement *cxt = (DbStatement *)v[0].instance->data;
  SET_INT_RETURN(sqlite3_column_count(cxt->st));
}

static void
c_Statement_bind_param(mrbc_vm *vm, mrbc_value v[], int argc)
{
  DbStatement *cxt = (DbStatement *)v[0].instance->data;
  int index = GET_INT_ARG(1);
  int status;
  switch (v[2].tt) {
    case MRBC_TT_FIXNUM:
      status = sqlite3_bind_int64(cxt->st, index, v[2].i);
      break;
    case MRBC_TT_FLOAT:
      status = sqlite3_bind_double(cxt->st, index, v[2].d);
      break;
    case MRBC_TT_STRING:
      status = sqlite3_bind_text(
        cxt->st,
        index,
        (const char *)v[2].string->data,
        (int)v[2].string->size,
        SQLITE_TRANSIENT
      );
      break;
    case MRBC_TT_NIL:
      status = sqlite3_bind_null(cxt->st, index);
      break;
    default:
      mrbc_raise(vm, MRBC_CLASS(TypeError), "no implicit conversion into String");
      break;
  }
  prb_sqlite3_raise(vm, sqlite3_db_handle(cxt->st), status);
  mrbc_incref(&v[0]);
  SET_RETURN(v[0]);
}

void
mrbc_init_class_SQLite3_Statement(void)
{
  mrbc_class *class_SQLite3 = mrbc_define_class(0, "SQLite3", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(class_SQLite3, mrbc_search_symid("Statement"));
  mrbc_class *class_SQLite3_Statement = v->cls;

  mrbc_define_method(0, class_SQLite3_Statement, "new", c_Statement_new);
  mrbc_define_method(0, class_SQLite3_Statement, "close", c_Statement_close);
  mrbc_define_method(0, class_SQLite3_Statement, "closed?", c_Statement_closed_q);
  mrbc_define_method(0, class_SQLite3_Statement, "step", c_Statement_step);
  mrbc_define_method(0, class_SQLite3_Statement, "reset!", c_Statement_reset_bang);
  mrbc_define_method(0, class_SQLite3_Statement, "done?", c_Statement_done_q);
  mrbc_define_method(0, class_SQLite3_Statement, "column_count", c_Statement_column_count);
  mrbc_define_method(0, class_SQLite3_Statement, "bind_param", c_Statement_bind_param);
}

