#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/*-------------------------------------
 *
 *  IO::Console
 *
 *------------------------------------*/

static bool raw_mode = false;
static bool raw_mode_saved = false;
static bool echo_mode = true;
static bool echo_mode_saved = true;

bool
io_raw_q(void)
{
  return raw_mode;
}

void
io_raw_bang(bool nonblock)
{
  (void)nonblock;
  raw_mode_saved = raw_mode;
  raw_mode = true;
}

void
io_cooked_bang(void)
{
  raw_mode_saved = raw_mode;
  raw_mode = false;
}

void
io_echo_eq(bool flag)
{
  echo_mode_saved = echo_mode;
  if (flag) {
    echo_mode = false;
  } else {
    echo_mode = true;
  }
}

bool
io_echo_q(void)
{
  return echo_mode;
}

void
io__restore_termios(void)
{
  raw_mode = raw_mode_saved;
  echo_mode = echo_mode_saved;
}
