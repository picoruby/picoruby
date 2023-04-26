#include "../include/sqlite3.h"

void
mrbc_init_class_SQLite3_ResultSet(void)
{
  mrbc_class *class_SQLite3 = mrbc_define_class(0, "SQLite3", mrbc_class_object);
  mrbc_value *v = mrbc_get_class_const(class_SQLite3, mrbc_search_symid("ResultSet"));
  mrbc_class *class_SQLite3_ResultSet = v->cls;
}

