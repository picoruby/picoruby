#include "../../include/platform.h"
#include <sys/utsname.h>
#include <stdio.h>

void
Platform_name(char *buf, size_t size)
{
  struct utsname u;
  uname(&u);
  snprintf(buf, size, "%s-%s", u.machine, u.sysname);
}
