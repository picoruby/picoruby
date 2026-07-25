#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/presym.h>
#include <mruby/string.h>
#include <mruby/variable.h>

static void
mrb_sqlite3_backup_free(mrb_state *mrb, void *ptr)
{
  DbBackup *bk = (DbBackup *)ptr;
  if (bk == NULL) return;
  if (bk->bk) {
    /* Finishing an unfinished backup reaches the VFS */
    prb_vfs_suspend_calls(true);
    sqlite3_backup_finish(bk->bk);
    prb_vfs_suspend_calls(false);
    bk->bk = NULL;
  }
  mrb_free(mrb, bk);
}

const struct mrb_data_type mrb_sqlite3_backup_type = {
  "SQLite3::Backup", mrb_sqlite3_backup_free,
};

static DbBackup *
backup(mrb_state *mrb, mrb_value self)
{
  return (DbBackup *)mrb_data_get_ptr(mrb, self, &mrb_sqlite3_backup_type);
}

static sqlite3 *
db_handle_of(mrb_state *mrb, mrb_value db_obj)
{
  DbState *state = (DbState *)mrb_data_get_ptr(mrb, db_obj, &mrb_sqlite3_database_type);
  if (state->closed || state->db == NULL) {
    mrb_raise(mrb, mrb_sqlite3_exception_class(mrb), "cannot use a closed database");
  }
  return state->db;
}

/*
 * SQLite3::Backup.new(dst_db, dst_name, src_db, src_name)
 *
 * `dst_name`/`src_name` are database names, usually "main". The two databases
 * commonly live on the same mounted volume; copying across volumes served by
 * different drivers is not supported by the VFS bridge.
 */
static mrb_value
mrb_s_backup_new(mrb_state *mrb, mrb_value klass)
{
  mrb_value dst_db, src_db;
  const char *dst_name, *src_name;
  mrb_get_args(mrb, "ozoz", &dst_db, &dst_name, &src_db, &src_name);

  sqlite3 *dst = db_handle_of(mrb, dst_db);
  sqlite3 *src = db_handle_of(mrb, src_db);

  DbBackup *bk = (DbBackup *)mrb_malloc(mrb, sizeof(DbBackup));
  bk->bk = NULL;
  mrb_value self = mrb_obj_value(
    Data_Wrap_Struct(mrb, mrb_class_ptr(klass), &mrb_sqlite3_backup_type, bk));

  bk->bk = sqlite3_backup_init(dst, dst_name, src, src_name);
  if (bk->bk == NULL) {
    /* The failure reason is recorded on the destination connection */
    prb_sqlite3_raise(mrb, dst, sqlite3_errcode(dst));
    /* prb_sqlite3_raise only raises for non-OK codes; guard the odd case */
    mrb_raise(mrb, mrb_sqlite3_exception_class(mrb), "could not initialize backup");
  }

  /* Keep both connections alive for the lifetime of the backup handle */
  mrb_iv_set(mrb, self, MRB_IVSYM(dst), dst_db);
  mrb_iv_set(mrb, self, MRB_IVSYM(src), src_db);
  return self;
}

static DbBackup *
open_backup(mrb_state *mrb, mrb_value self)
{
  DbBackup *bk = backup(mrb, self);
  if (bk->bk == NULL) {
    mrb_raise(mrb, mrb_sqlite3_exception_class(mrb), "backup is already finished");
  }
  return bk;
}

/*
 * step(pages) -> Integer
 *
 * Copies up to `pages` pages (a negative value copies all remaining pages).
 * Returns SQLite3::Backup::DONE when finished, ::OK when more pages remain, or
 * ::BUSY / ::LOCKED when the source is momentarily unavailable. Any other
 * result raises.
 */
static mrb_value
mrb_backup_step(mrb_state *mrb, mrb_value self)
{
  mrb_int pages;
  mrb_get_args(mrb, "i", &pages);
  sqlite3_backup *bk = open_backup(mrb, self)->bk;

  int rc = sqlite3_backup_step(bk, (int)pages);
  switch (rc) {
    case SQLITE_OK:
    case SQLITE_DONE:
    case SQLITE_BUSY:
    case SQLITE_LOCKED:
      return mrb_fixnum_value(rc);
    default:
      prb_sqlite3_raise(mrb, NULL, rc);
      return mrb_nil_value();
  }
}

static mrb_value
mrb_backup_finish(mrb_state *mrb, mrb_value self)
{
  DbBackup *bk = backup(mrb, self);
  if (bk->bk) {
    sqlite3_backup_finish(bk->bk);
    bk->bk = NULL;
  }
  return mrb_nil_value();
}

static mrb_value
mrb_backup_remaining(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(sqlite3_backup_remaining(open_backup(mrb, self)->bk));
}

static mrb_value
mrb_backup_pagecount(mrb_state *mrb, mrb_value self)
{
  return mrb_fixnum_value(sqlite3_backup_pagecount(open_backup(mrb, self)->bk));
}

void
mrb_init_class_SQLite3_Backup(mrb_state *mrb, struct RClass *class_SQLite3)
{
  struct RClass *class_SQLite3_Backup = mrb_define_class_under_id(
    mrb, class_SQLite3, MRB_SYM(Backup), mrb->object_class);
  MRB_SET_INSTANCE_TT(class_SQLite3_Backup, MRB_TT_CDATA);

  /* step() return codes, so callers can drive the loop CRuby style */
  mrb_define_const_id(mrb, class_SQLite3_Backup, MRB_SYM(OK), mrb_fixnum_value(SQLITE_OK));
  mrb_define_const_id(mrb, class_SQLite3_Backup, MRB_SYM(DONE), mrb_fixnum_value(SQLITE_DONE));
  mrb_define_const_id(mrb, class_SQLite3_Backup, MRB_SYM(BUSY), mrb_fixnum_value(SQLITE_BUSY));
  mrb_define_const_id(mrb, class_SQLite3_Backup, MRB_SYM(LOCKED), mrb_fixnum_value(SQLITE_LOCKED));

  mrb_define_class_method_id(mrb, class_SQLite3_Backup, MRB_SYM(new), mrb_s_backup_new, MRB_ARGS_REQ(4));
  mrb_define_method_id(mrb, class_SQLite3_Backup, MRB_SYM(step), mrb_backup_step, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_SQLite3_Backup, MRB_SYM(finish), mrb_backup_finish, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Backup, MRB_SYM(remaining), mrb_backup_remaining, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_SQLite3_Backup, MRB_SYM(pagecount), mrb_backup_pagecount, MRB_ARGS_NONE());
}
