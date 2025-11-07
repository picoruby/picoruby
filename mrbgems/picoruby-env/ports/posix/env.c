#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

extern char **environ;

void
ENV_get_key_value(char **key, size_t *key_len, char **value, size_t *value_len)
{
  static char **my_environ;
  static bool first = true;

  if (first) {
    my_environ = environ;
    first = false;
  }

  if (my_environ == NULL || *my_environ == NULL) {
    *key = NULL;
    *key_len = 0;
    *value = NULL;
    *value_len = 0;
    return;
  }

  char *eq_pos = strchr(*my_environ, '=');
  if (eq_pos == NULL) {
    *key = *my_environ;
    *key_len = strlen(*my_environ);
    *value = NULL;
    *value_len = 0;
  } else {
    *key = *my_environ;
    *key_len = eq_pos - *my_environ;
    *value = eq_pos + 1;
    *value_len = strlen(*value);
  }

  my_environ++;
}

int
ENV_unsetenv(const char *name)
{
  return unsetenv(name);
}

int
ENV_setenv(const char *name, const char *value, int override)
{
#if defined(_BSD_SOURCE) || \
    (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600)
  return setenv(name, value, 1);
#elif defined(_SVID_SOURCE) || (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE < 600)
  char *buf = mrbc_raw_alloc_no_free(strlen(name) + strlen(value) + 2);
  sprintf(buf, "%s=%s", name, value);
  return putenv(buf);
#endif
  return setenv(name, value, override);
}
