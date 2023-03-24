#include <mrubyc.h>

#include "../lib/sqlite-amalgamation-3410100/sqlite3.h"

static void
c_(mrbc_vm *vm, mrbc_value v[], int argc)
{
}

void
mrbc_sqlite3_init(void)
{
  mrbc_class *class_SQLite3 = mrbc_define_class(0, "SQLite3", mrbc_class_object);
  mrbc_define_method(0, class_FAT, "", c_);
}
