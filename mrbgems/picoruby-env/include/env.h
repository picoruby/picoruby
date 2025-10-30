#ifndef ENV_DEFINED_H_
#define ENV_DEFINED_H_

void ENV_get_key_value(char **key, size_t *key_len, char **value, size_t *value_len);
int ENV_unsetenv(const char *name);
int ENV_setenv(const char *name, const char *value, int override);

#endif

