/* nRF52: process environment variables do not exist on bare-metal.
 * All functions are intentional NOPs. When implementing, consider persisting
 * key-value pairs via littlefs (e.g. /etc/env) as the simplest option. */
#include <stddef.h>

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
  (void)name;
  return 0;
}

int
ENV_setenv(const char *name, const char *value, int override)
{
  (void)name;
  (void)value;
  (void)override;
  return 0;
}
