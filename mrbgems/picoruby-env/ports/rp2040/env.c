#include <stddef.h>

void
ENV_get_key_value(char **key, char **value)
{
  *key = NULL;
  *value = NULL;
}

int
ENV_unsetenv(const char *name)
{
  // Unsetting environment variables is not supported in this environment.
  (void)name;
  return 0;
}

int
ENV_setenv(const char *name, const char *value, int override)
{
  // Setting environment variables is not supported in this environment.
  // The override parameter is ignored.
  (void)name;
  (void)value;
  (void)override;
  return 0;
}
