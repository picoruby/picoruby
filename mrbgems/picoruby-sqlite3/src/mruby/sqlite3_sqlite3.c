#include <mruby/class.h>
#include <mruby/presym.h>

struct RClass *
mrb_sqlite3_exception_class(mrb_state *mrb)
{
  struct RClass *class_SQLite3 = mrb_class_get_id(mrb, MRB_SYM(SQLite3));
  return mrb_class_get_under_id(mrb, class_SQLite3, MRB_SYM(Exception));
}

struct RClass *
mrb_sqlite3_exception_class_for(mrb_state *mrb, int status)
{
  /* All subclasses are defined in gem_init below, so the lookup always
     succeeds; presym symbols keep this free of runtime string interning. */
  struct RClass *class_SQLite3 = mrb_class_get_id(mrb, MRB_SYM(SQLite3));
  mrb_sym sym;
  switch (status & 0xFF) {
    case SQLITE_CONSTRAINT: sym = MRB_SYM(ConstraintException); break;
    case SQLITE_BUSY:       sym = MRB_SYM(BusyException);       break;
    case SQLITE_READONLY:   sym = MRB_SYM(ReadOnlyException);   break;
    case SQLITE_FULL:       sym = MRB_SYM(FullException);       break;
    case SQLITE_CORRUPT:    sym = MRB_SYM(CorruptException);    break;
    case SQLITE_IOERR:      sym = MRB_SYM(IOException);         break;
    case SQLITE_CANTOPEN:   sym = MRB_SYM(CantOpenException);   break;
    default:                return mrb_sqlite3_exception_class(mrb);
  }
  return mrb_class_get_under_id(mrb, class_SQLite3, sym);
}

void
mrb_picoruby_sqlite3_gem_init(mrb_state *mrb)
{
  struct RClass *class_SQLite3 = mrb_define_class_id(mrb, MRB_SYM(SQLite3), mrb->object_class);

  /* Defined here rather than in mrblib so that the C code can always raise it.
     The subset of subclasses mirrors the sqlite3 gem for the result codes that
     matter most on a microcontroller (constraint violations plus the flash
     failure modes: full, corrupt, I/O error). */
  struct RClass *class_Exception =
    mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(Exception), mrb->eStandardError_class);
  mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(ConstraintException), class_Exception);
  mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(BusyException), class_Exception);
  mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(ReadOnlyException), class_Exception);
  mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(FullException), class_Exception);
  mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(CorruptException), class_Exception);
  mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(IOException), class_Exception);
  mrb_define_class_under_id(mrb, class_SQLite3, MRB_SYM(CantOpenException), class_Exception);

  /* SQLite3::Constants::Open: the flags of sqlite3_open_v2(), named as the
     sqlite3 gem names them */
  struct RClass *mod_Constants = mrb_define_module_under_id(mrb, class_SQLite3, MRB_SYM(Constants));
  struct RClass *mod_Open = mrb_define_module_under_id(mrb, mod_Constants, MRB_SYM(Open));
#define OPEN_CONST(name) \
  mrb_define_const_id(mrb, mod_Open, MRB_SYM(name), mrb_int_value(mrb, SQLITE_OPEN_##name))
  OPEN_CONST(READONLY);
  OPEN_CONST(READWRITE);
  OPEN_CONST(CREATE);
  OPEN_CONST(DELETEONCLOSE);
  OPEN_CONST(EXCLUSIVE);
  OPEN_CONST(AUTOPROXY);
  OPEN_CONST(URI);
  OPEN_CONST(MEMORY);
  OPEN_CONST(MAIN_DB);
  OPEN_CONST(TEMP_DB);
  OPEN_CONST(TRANSIENT_DB);
  OPEN_CONST(MAIN_JOURNAL);
  OPEN_CONST(TEMP_JOURNAL);
  OPEN_CONST(SUBJOURNAL);
  OPEN_CONST(SUPER_JOURNAL);
  OPEN_CONST(MASTER_JOURNAL);
  OPEN_CONST(NOMUTEX);
  OPEN_CONST(FULLMUTEX);
  OPEN_CONST(SHAREDCACHE);
  OPEN_CONST(PRIVATECACHE);
  OPEN_CONST(WAL);
  OPEN_CONST(NOFOLLOW);
#undef OPEN_CONST

  mrb_init_class_SQLite3_Database(mrb, class_SQLite3);
  mrb_init_class_SQLite3_Statement(mrb, class_SQLite3);
  mrb_init_class_SQLite3_Backup(mrb, class_SQLite3);
}

void
mrb_picoruby_sqlite3_gem_final(mrb_state *mrb)
{
  prb_vfs_forget_driver(mrb);
}
