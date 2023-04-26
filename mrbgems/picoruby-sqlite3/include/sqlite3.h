#ifndef SQLITE3_DEFINED_H_
#define SQLITE3_DEFINED_H_

#include <stdbool.h>
#include <mrubyc.h>
#include "sqlite3_vfs.h"
#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  sqlite3 *db;
  bool closed;
} DbState;

void mrbc_init_class_SQLite3_Database(void);
void mrbc_init_class_SQLite3_Statement(void);
void mrbc_init_class_SQLite3_ResultSet(void);

#ifdef __cplusplus
}
#endif

#endif /* SQLITE3_DEFINED_H_ */

