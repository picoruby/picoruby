#ifdef MRBC_USE_HAL_POSIX
#include "hal/posix/io_console.h"
#endif

void
mrbc_terminal_init(void)
{
  mrbc_io_console_init();
}

