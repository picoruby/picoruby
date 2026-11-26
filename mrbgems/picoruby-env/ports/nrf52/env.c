/*
 * nRF52 port: environment variables persisted to littlefs at /etc/env.
 *
 * There is no process environment on bare metal, so ENV is an in-RAM hash
 * that the gem builds at boot by calling ENV_get_key_value until it runs
 * out of pairs. That enumeration is the read path: filling it from a file
 * is what makes assignments survive a reboot.
 *
 * The file is a flat KEY=VALUE per line. Whole-file rewrite on every
 * change, because an environment is a handful of short strings and the
 * alternative -- in-place edits with a free list -- is a filesystem, which
 * littlefs already is.
 *
 * Reaching another gem's C API is done the way picoruby-uart reaches
 * picoruby-irq: a declared dependency plus an include path in mrbgem.rake,
 * not a relative include.
 */

#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#include "littlefs.h"

#include "../../include/env.h"

#define ENV_PATH      "/etc/env"
#define ENV_DIR       "/etc"
/* An environment that outgrows this is not an environment any more. */
#define ENV_BLOB_MAX  512
#define ENV_NAME_MAX  64
#define ENV_VALUE_MAX 192

static long timezone_offset;

/* The enumeration hands out pointers the caller copies immediately, so
   one buffer per field is enough. */
static char blob[ENV_BLOB_MAX + 1];
static size_t blob_len;
static size_t enum_pos;
static bool   enum_active;
static char   enum_key[ENV_NAME_MAX + 1];
static char   enum_value[ENV_VALUE_MAX + 1];

static int
parse_timezone_offset(const char *value, long *offset)
{
  const char *p = value;
  int sign = 1;
  long hours = 0;
  long minutes = 0;

  while (isalpha((unsigned char)*p)) p++;
  if (*p == '+' || *p == '-') {
    sign = *p == '-' ? -1 : 1;
    p++;
  }
  if (!isdigit((unsigned char)*p)) return -1;
  while (isdigit((unsigned char)*p)) {
    hours = hours * 10 + (*p++ - '0');
  }
  if (*p == ':') {
    p++;
    if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return -1;
    minutes = (p[0] - '0') * 10 + p[1] - '0';
    p += 2;
  }
  if (*p != '\0' || minutes >= 60) return -1;

  /* The conventional seconds-west offset Time expects. */
  *offset = sign * (hours * 3600 + minutes * 60);
  return 0;
}

static bool
load_blob(void)
{
  lfs_file_t file;

  blob_len = 0;
  blob[0] = '\0';

  if (littlefs_ensure_mounted() != LFS_ERR_OK) {
    return false;
  }
  lfs_t *lfs = littlefs_get_lfs();
  if (lfs_file_open(lfs, &file, ENV_PATH, LFS_O_RDONLY) != LFS_ERR_OK) {
    return false;  /* nothing persisted yet is not an error */
  }
  lfs_ssize_t read = lfs_file_read(lfs, &file, blob, ENV_BLOB_MAX);
  lfs_file_close(lfs, &file);

  if (read < 0) {
    return false;
  }
  blob_len = (size_t)read;
  blob[blob_len] = '\0';
  return true;
}

static bool
store_blob(void)
{
  lfs_file_t file;

  if (littlefs_ensure_mounted() != LFS_ERR_OK) {
    return false;
  }
  lfs_t *lfs = littlefs_get_lfs();

  /* /etc may not exist on a freshly formatted volume. Already-there is
     the expected answer, not a failure. */
  int err = lfs_mkdir(lfs, ENV_DIR);
  if (err != LFS_ERR_OK && err != LFS_ERR_EXIST) {
    return false;
  }
  if (lfs_file_open(lfs, &file, ENV_PATH,
                    LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC) != LFS_ERR_OK) {
    return false;
  }
  lfs_ssize_t written = lfs_file_write(lfs, &file, blob, blob_len);
  int closed = lfs_file_close(lfs, &file);
  return (written == (lfs_ssize_t)blob_len) && (closed == LFS_ERR_OK);
}

/* Start of the KEY=... line for `name`, or NULL. */
static char *
find_line(const char *name)
{
  size_t name_len = strlen(name);
  char *p = blob;

  while (*p != '\0') {
    if (strncmp(p, name, name_len) == 0 && p[name_len] == '=') {
      return p;
    }
    char *newline = strchr(p, '\n');
    if (newline == NULL) {
      break;
    }
    p = newline + 1;
  }
  return NULL;
}

static void
remove_line(char *line)
{
  char *next = strchr(line, '\n');

  if (next == NULL) {
    blob_len = (size_t)(line - blob);
  } else {
    next++;
    size_t tail = blob_len - (size_t)(next - blob);
    memmove(line, next, tail);
    blob_len -= (size_t)(next - line);
  }
  blob[blob_len] = '\0';
}

void
ENV_get_key_value(char **key, size_t *key_len, char **value, size_t *value_len)
{
  *key = NULL;
  *key_len = 0;
  *value = NULL;
  *value_len = 0;

  /* The gem calls this in a loop until it gets a NULL key, so the first
     call of each sweep is the one that (re)reads the file. */
  if (!enum_active) {
    load_blob();
    enum_pos = 0;
    enum_active = true;
  }

  while (enum_pos < blob_len) {
    char *line = &blob[enum_pos];
    char *newline = strchr(line, '\n');
    size_t line_len = (newline != NULL) ? (size_t)(newline - line)
                                        : (blob_len - enum_pos);
    enum_pos += line_len + (newline != NULL ? 1 : 0);

    char *eq = memchr(line, '=', line_len);
    if (eq == NULL) {
      continue;  /* not a KEY=VALUE line */
    }
    size_t k_len = (size_t)(eq - line);
    size_t v_len = line_len - k_len - 1;
    if (k_len == 0 || k_len > ENV_NAME_MAX || v_len > ENV_VALUE_MAX) {
      continue;
    }

    memcpy(enum_key, line, k_len);
    enum_key[k_len] = '\0';
    memcpy(enum_value, eq + 1, v_len);
    enum_value[v_len] = '\0';

    /* picoruby-time reads the offset through ENV_get_timezone_offset, not
       through the hash, so it has to be parsed as it goes past. */
    if (strcmp(enum_key, "TZ") == 0) {
      long offset;
      if (parse_timezone_offset(enum_value, &offset) == 0) {
        timezone_offset = offset;
      }
    }

    *key = enum_key;
    *key_len = k_len;
    *value = enum_value;
    *value_len = v_len;
    return;
  }

  /* Sweep finished. Re-arm so a later one starts from the file again. */
  enum_active = false;
}

int
ENV_setenv(const char *name, const char *value, int override)
{
  if (name == NULL || value == NULL) {
    return -1;
  }
  size_t name_len = strlen(name);
  size_t value_len = strlen(value);
  if (name_len == 0 || name_len > ENV_NAME_MAX || value_len > ENV_VALUE_MAX) {
    return -1;
  }
  if (strchr(name, '=') != NULL || strchr(name, '\n') != NULL ||
      strchr(value, '\n') != NULL) {
    return -1;  /* would not survive the round trip */
  }

  if (strcmp(name, "TZ") == 0) {
    long offset;
    if (parse_timezone_offset(value, &offset) == 0) {
      timezone_offset = offset;
    }
  }

  if (!load_blob()) {
    blob_len = 0;
    blob[0] = '\0';
  }

  char *existing = find_line(name);
  if (existing != NULL) {
    if (!override) {
      return 0;
    }
    remove_line(existing);
  }

  size_t needed = name_len + 1 + value_len + 1;   /* KEY=VALUE\n */
  if (blob_len + needed > ENV_BLOB_MAX) {
    return -1;
  }
  memcpy(&blob[blob_len], name, name_len);
  blob_len += name_len;
  blob[blob_len++] = '=';
  memcpy(&blob[blob_len], value, value_len);
  blob_len += value_len;
  blob[blob_len++] = '\n';
  blob[blob_len] = '\0';

  return store_blob() ? 0 : -1;
}

int
ENV_unsetenv(const char *name)
{
  if (name == NULL) {
    return -1;
  }
  if (strcmp(name, "TZ") == 0) {
    timezone_offset = 0;
  }

  if (!load_blob()) {
    return 0;   /* nothing stored, nothing to remove */
  }
  char *existing = find_line(name);
  if (existing == NULL) {
    return 0;
  }
  remove_line(existing);
  return store_blob() ? 0 : -1;
}

long
ENV_get_timezone_offset(void)
{
  return timezone_offset;
}
