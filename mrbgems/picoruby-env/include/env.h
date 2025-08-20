#ifndef ENV_DEFINED_H_
#define ENV_DEFINED_H_

void ENV_get_key_value(char **key, char **value);
int ENV_unsetenv(const char *name);
int ENV_setenv(const char *name, const char *value, int override);

#endif

