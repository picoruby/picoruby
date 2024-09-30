#ifndef SQLITE3_DEFINED_H_
#define SQLITE3_DEFINED_H_

#include <stdbool.h>
#include <mrubyc.h>
#include "sqlite3_vfs_methods.h"
#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MRBC_INT64
#define MRBC_INT64 1
#endif

typedef struct {
  sqlite3 *db;
  bool closed;
} DbState;

void mrbc_init_class_SQLite3_Database(mrbc_vm *vm, mrbc_class *class_SQLite3);
void mrbc_init_class_SQLite3_Statement(mrbc_vm *vm, mrbc_class *class_SQLite3);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_DEFINED_H_ */

