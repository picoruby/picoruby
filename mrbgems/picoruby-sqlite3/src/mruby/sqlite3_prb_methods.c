#include <string.h>

#include <mruby/class.h>
#include <mruby/error.h>
#include <mruby/presym.h>
#include <mruby/string.h>

#define SEEK_SET 0

/* Used when the driver does not report a sector size */
#define DEFAULT_SECTOR_SIZE 512

/*
 * SQLite registers one global VFS, so there is one driver object at a time,
 * just as there is one mrb_state. The driver is a picoruby-vfs driver such as
 * a Littlefs instance; it is GC-registered for as long as SQLite may call it.
 */
static mrb_state *vfs_mrb = NULL;
static mrb_value vfs_driver;
static bool vfs_calls_suspended = false;

void
prb_vfs_suspend_calls(bool suspend)
{
  vfs_calls_suspended = suspend;
}

bool
prb_vfs_driver_set_p(void)
{
  return vfs_mrb != NULL;
}

void
prb_vfs_set_driver(mrb_state *mrb, mrb_value driver)
{
  if (vfs_mrb != NULL) {
    if (mrb_obj_eq(mrb, vfs_driver, driver)) return;
    mrb_gc_unregister(vfs_mrb, vfs_driver);
  }
  vfs_mrb = mrb;
  vfs_driver = driver;
  mrb_gc_register(mrb, driver);
}

void
prb_vfs_forget_driver(mrb_state *mrb)
{
  if (vfs_mrb == NULL) return;
  mrb_gc_unregister(vfs_mrb, vfs_driver);
  vfs_driver = mrb_nil_value();
  vfs_mrb = NULL;
}

typedef struct {
  mrb_value recv;
  mrb_sym mid;
  mrb_int argc;
  const mrb_value *argv;
} prb_call_args;

static mrb_value
prb_call_body(mrb_state *mrb, void *ud)
{
  prb_call_args *a = (prb_call_args *)ud;
  return mrb_funcall_argv(mrb, a->recv, a->mid, a->argc, a->argv);
}

/*
 * Calls a Ruby method on behalf of SQLite. SQLite's C frames sit between the
 * VM and this call, so an exception must not longjmp past them; it is caught
 * here and reported to the caller as mrb_undef_value().
 */
static mrb_value
prb_call(mrb_value recv, mrb_sym mid, mrb_int argc, const mrb_value *argv)
{
  mrb_state *mrb = vfs_mrb;
  if (mrb == NULL || vfs_calls_suspended) return mrb_undef_value();
  prb_call_args a = { recv, mid, argc, argv };
  mrb_bool error = FALSE;
  mrb_value ret = mrb_protect_error(mrb, prb_call_body, &a, &error);
  return error ? mrb_undef_value() : ret;
}

/* Integer result of a driver call, or -1 when the call did not succeed */
static mrb_int
prb_call_int(mrb_value recv, mrb_sym mid, mrb_int argc, const mrb_value *argv)
{
  mrb_value ret = prb_call(recv, mid, argc, argv);
  return mrb_integer_p(ret) ? mrb_integer(ret) : -1;
}

int64_t
prb_time_gettime_us(void)
{
  mrb_state *mrb = vfs_mrb;
  if (mrb == NULL) return 0;

  int ai = mrb_gc_arena_save(mrb);
  int64_t usec = 0;
  mrb_value time_class = mrb_obj_value(mrb_class_get_id(mrb, MRB_SYM(Time)));
  mrb_value now = prb_call(time_class, MRB_SYM(now), 0, NULL);
  if (!mrb_undef_p(now)) {
    mrb_value sec = prb_call(now, MRB_SYM(to_i), 0, NULL);
    mrb_value frac = prb_call(now, MRB_SYM(usec), 0, NULL);
    if (mrb_integer_p(sec) && mrb_integer_p(frac)) {
      usec = (int64_t)mrb_integer(sec) * 1000000 + (int64_t)mrb_integer(frac);
    }
  }
  mrb_gc_arena_restore(mrb, ai);
  return usec;
}

static int
prb_file_query_sector_size(mrb_value file)
{
  mrb_int size = prb_call_int(file, MRB_SYM(sector_size), 0, NULL);
  return (0 < size) ? (int)size : DEFAULT_SECTOR_SIZE;
}

int
prb_file_new(PRBFile *prbfile, const char *zName, int flags)
{
  mrb_state *mrb = vfs_mrb;
  if (mrb == NULL) return -1;
  /* SQLite passes NULL to ask for an anonymous temporary file; there is no
     name to hand the driver, so report that it cannot be opened */
  if (zName == NULL) return -1;

  const char *mode;
  if (flags & SQLITE_OPEN_CREATE) {
    mode = prb_file_exist_q(zName) ? "r+" : "w+";
  } else {
    mode = "r";
  }

  int ai = mrb_gc_arena_save(mrb);
  mrb_value argv[2];
  argv[0] = mrb_str_new_cstr(mrb, zName);
  argv[1] = mrb_str_new_cstr(mrb, mode);
  mrb_value file = prb_call(vfs_driver, MRB_SYM(open_file), 2, argv);
  int ret = -1;
  if (!mrb_undef_p(file) && !mrb_nil_p(file)) {
    /* The GC does not scan PRBFile, so hold the File object explicitly */
    mrb_gc_register(mrb, file);
    prbfile->mrb = mrb;
    prbfile->file = file;
    prbfile->sector_size = prb_file_query_sector_size(file);
    ret = 0;
  }
  mrb_gc_arena_restore(mrb, ai);
  return ret;
}

int
prb_file_close(PRBFile *prbfile)
{
  mrb_state *mrb = prbfile->mrb;
  if (mrb == NULL) return 0;
  if (vfs_calls_suspended) {
    /*
     * Reached from a GC free function. Neither closing the File nor dropping
     * its GC root may run here, so the File is left registered and littlefs
     * releases the handle from its own free function instead.
     */
    prbfile->mrb = NULL;
    return 0;
  }

  int ai = mrb_gc_arena_save(mrb);
  prb_call(prbfile->file, MRB_SYM(close), 0, NULL);
  mrb_gc_arena_restore(mrb, ai);

  mrb_gc_unregister(mrb, prbfile->file);
  prbfile->file = mrb_nil_value();
  prbfile->mrb = NULL;
  return 0;
}

int
prb_file_read(PRBFile *prbfile, void *zBuf, size_t nBuf)
{
  mrb_state *mrb = prbfile->mrb;
  if (mrb == NULL) return -1;

  int ai = mrb_gc_arena_save(mrb);
  mrb_value arg = mrb_int_value(mrb, (mrb_int)nBuf);
  mrb_value ret = prb_call(prbfile->file, MRB_SYM(read), 1, &arg);
  int nRead;
  if (mrb_nil_p(ret)) {
    nRead = 0; /* EOF */
  } else if (mrb_string_p(ret)) {
    size_t size = (size_t)RSTRING_LEN(ret);
    if (nBuf < size) size = nBuf;
    memcpy(zBuf, RSTRING_PTR(ret), size);
    nRead = (int)size;
  } else {
    nRead = -1;
  }
  mrb_gc_arena_restore(mrb, ai);
  return nRead;
}

int
prb_file_write(PRBFile *prbfile, const void *zBuf, size_t nBuf)
{
  mrb_state *mrb = prbfile->mrb;
  if (mrb == NULL) return -1;

  int ai = mrb_gc_arena_save(mrb);
  mrb_value arg = mrb_str_new(mrb, (const char *)zBuf, (mrb_int)nBuf);
  int nWritten = (int)prb_call_int(prbfile->file, MRB_SYM(write), 1, &arg);
  mrb_gc_arena_restore(mrb, ai);
  return nWritten;
}

int
prb_file_fsync(PRBFile *prbfile)
{
  if (prbfile->mrb == NULL) return -1;
  return (prb_call_int(prbfile->file, MRB_SYM(fsync), 0, NULL) < 0) ? -1 : 0;
}

int
prb_file_seek(PRBFile *prbfile, sqlite3_int64 offset)
{
  mrb_state *mrb = prbfile->mrb;
  if (mrb == NULL) return -1;

  int ai = mrb_gc_arena_save(mrb);
  mrb_value argv[2];
  argv[0] = mrb_int_value(mrb, (mrb_int)offset);
  argv[1] = mrb_fixnum_value(SEEK_SET);
  mrb_int ret = prb_call_int(prbfile->file, MRB_SYM(seek), 2, argv);
  mrb_gc_arena_restore(mrb, ai);
  return (ret < 0) ? -1 : 0;
}

int
prb_file_tell(PRBFile *prbfile)
{
  if (prbfile->mrb == NULL) return -1;
  return (int)prb_call_int(prbfile->file, MRB_SYM(tell), 0, NULL);
}

sqlite3_int64
prb_file_size(PRBFile *prbfile)
{
  if (prbfile->mrb == NULL) return -1;
  return (sqlite3_int64)prb_call_int(prbfile->file, MRB_SYM(size), 0, NULL);
}

int
prb_file_unlink(const char *zName)
{
  mrb_state *mrb = vfs_mrb;
  if (mrb == NULL) return -1;

  int ai = mrb_gc_arena_save(mrb);
  mrb_value arg = mrb_str_new_cstr(mrb, zName);
  mrb_value ret = prb_call(vfs_driver, MRB_SYM(unlink), 1, &arg);
  mrb_gc_arena_restore(mrb, ai);
  return mrb_undef_p(ret) ? -1 : 0;
}

bool
prb_file_exist_q(const char *zName)
{
  mrb_state *mrb = vfs_mrb;
  if (mrb == NULL) return false;

  int ai = mrb_gc_arena_save(mrb);
  mrb_value arg = mrb_str_new_cstr(mrb, zName);
  mrb_value ret = prb_call(vfs_driver, MRB_SYM_Q(exist), 1, &arg);
  /* mrb_test() would report undef as truthy, so check for it first */
  bool exist = !mrb_undef_p(ret) && mrb_test(ret);
  mrb_gc_arena_restore(mrb, ai);
  return exist;
}

/*
 * SQLite requires xSize to report the usable size of an allocation and
 * requires allocations to be 8-byte aligned. The PicoRuby heap answers
 * neither question, so the requested size is stored in a header that is wide
 * enough to preserve the alignment.
 */
typedef union {
  uint64_t align;
  size_t size;
} prb_mem_header;

void *
prb_mem_alloc(int nByte)
{
  mrb_state *mrb = vfs_mrb;
  if (mrb == NULL || nByte <= 0) return NULL;
  prb_mem_header *p = (prb_mem_header *)mrb_malloc_simple(
    mrb, sizeof(prb_mem_header) + (size_t)nByte);
  if (p == NULL) return NULL;
  p->size = (size_t)nByte;
  return (void *)(p + 1);
}

void
prb_mem_free(void *pPrior)
{
  mrb_state *mrb = vfs_mrb;
  if (mrb == NULL || pPrior == NULL) return;
  mrb_free(mrb, (prb_mem_header *)pPrior - 1);
}

void *
prb_mem_realloc(void *pPrior, int nByte)
{
  mrb_state *mrb = vfs_mrb;
  if (mrb == NULL) return NULL;
  if (pPrior == NULL) return prb_mem_alloc(nByte);
  if (nByte <= 0) {
    prb_mem_free(pPrior);
    return NULL;
  }
  prb_mem_header *p = (prb_mem_header *)mrb_realloc_simple(
    mrb, (prb_mem_header *)pPrior - 1, sizeof(prb_mem_header) + (size_t)nByte);
  if (p == NULL) return NULL;
  p->size = (size_t)nByte;
  return (void *)(p + 1);
}

int
prb_mem_size(void *p)
{
  if (p == NULL) return 0;
  return (int)((prb_mem_header *)p - 1)->size;
}

int
prb_mem_roundup(int n)
{
  return (n + 7) & ~7;
}

int
prb_mem_init(void *ignore)
{
  return SQLITE_OK;
}

void
prb_mem_shutdown(void *ignore)
{
}
