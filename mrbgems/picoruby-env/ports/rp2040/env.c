/*
 * RP2040 has no process environment.  Keep ordinary variables as NOPs because
 * newlib's setenv/unsetenv implementation touches the heap, which is unsafe
 * for the firmware's memory model.  TZ is parsed into a small static offset
 * because picoruby-time needs it for local time conversion.
 */
#include <stddef.h>
#include <ctype.h>
#include <string.h>

static long timezone_offset;

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

  /* Store the conventional seconds-west offset used by Time. */
  *offset = sign * (hours * 3600 + minutes * 60);
  return 0;
}

void
ENV_get_key_value(char **key, size_t *key_len, char **value, size_t *value_len)
{
  *key = NULL;
  *key_len = 0;
  *value = NULL;
  *value_len = 0;
}

int
ENV_unsetenv(const char *name)
{
  if (strcmp(name, "TZ") == 0) timezone_offset = 0;
  (void)name;
  return 0;
}

int
ENV_setenv(const char *name, const char *value, int override)
{
  long offset;
  if (strcmp(name, "TZ") == 0 && parse_timezone_offset(value, &offset) == 0) {
    timezone_offset = offset;
  }
  (void)name;
  (void)value;
  (void)override;
  return 0;
}

long
ENV_get_timezone_offset(void)
{
  return timezone_offset;
}
