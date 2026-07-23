#ifndef SQLITE3_DEFINED_H_
#define SQLITE3_DEFINED_H_

#include <stdbool.h>

#if defined(PICORB_VM_MRUBY)
  #include <mruby.h>
#endif

#include "sqlite3_vfs_methods.h"
#include "../lib/sqlite-amalgamation-3530300/sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * sqlite3.h only declares the sqlite3 struct; its definition lives in
 * sqlite3.c, which is not included here.
 */
typedef struct {
  sqlite3 *db;
  bool closed;
} DbState;

typedef struct {
  sqlite3_stmt *st;
  bool done_p;
} DbStatement;

#if defined(PICORB_VM_MRUBY)

extern const struct mrb_data_type mrb_sqlite3_database_type;
extern const struct mrb_data_type mrb_sqlite3_statement_type;

struct RClass *mrb_sqlite3_exception_class(mrb_state *mrb);
void prb_sqlite3_raise(mrb_state *mrb, sqlite3 *db, int status);

void mrb_init_class_SQLite3_Database(mrb_state *mrb, struct RClass *class_SQLite3);
void mrb_init_class_SQLite3_Statement(mrb_state *mrb, struct RClass *class_SQLite3);

#endif

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_DEFINED_H_ */
