#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

typedef struct shell_executables {
  const char *path;
  const uint8_t *vm_code;
  const uint32_t crc;
} shell_executables;

#include "shell_executables.c.inc"

#if defined(PICORB_VM_MRUBY)

#include "mruby/shell.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/shell.c"

#endif
