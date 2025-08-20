#include <stddef.h>
#include <string.h>
#include <stdlib.h>

extern char **environ;

void
ENV_get_key_value(char **key, char **value)
{
  static char **my_environ;
  static int first = 1;
  if (first) {
    my_environ = environ;
    first = 0;
  }
  if (my_environ == NULL) {
    return;
  }
  *key = strtok(*my_environ, "=");
  *value = strtok(NULL, "=");
  my_environ++;
}

int ENV_unsetenv(const char *name)
{
  return unsetenv(name);
}

int ENV_setenv(const char *name, const char *value, int override)
{
  return setenv(name, value, override);
}
