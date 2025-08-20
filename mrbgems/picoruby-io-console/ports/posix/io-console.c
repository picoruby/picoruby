#include <stdio.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include "../../../picoruby-machine/include/machine.h"

static struct termios save_settings;
static int save_flags;

int
hal_getchar(void)
{
  if (sigint_status == MACHINE_SIGINT_RECEIVED) {
    sigint_status = MACHINE_SIG_NONE;
    return 3; // Ctrl-C
  } else if (sigint_status == MACHINE_SIGTSTP_RECEIVED) {
    sigint_status = MACHINE_SIG_NONE;
    return 26; // Ctrl-Z
  }
  int c = getchar();
  if (c == EOF) {
    return -1;
  } else {
    return c;
  }
}

bool
io_raw_q(void)
{
  struct termios settings;
  tcgetattr(fileno(stdin), &settings);
  if ((settings.c_iflag & (BRKINT | ISTRIP | IXON)) == 0 &&
      (settings.c_lflag & (ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHONL)) == 0) {
    return true;
  }
  else {
    return false;
  }
}

void
io_raw_bang(bool nonblock)
{
  struct termios settings;
  tcgetattr(fileno(stdin), &save_settings);
  settings = save_settings;
  settings.c_iflag &= ~(BRKINT | ISTRIP | IXON);
  settings.c_lflag &= ~(ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHONL);
  settings.c_cc[VMIN]  = 1;
  settings.c_cc[VTIME] = 0;
  tcsetattr(fileno(stdin), TCSANOW, &settings);
  save_flags = fcntl(fileno(stdin), F_GETFL, 0);
  if (nonblock) {
    fcntl(fileno(stdin), F_SETFL, save_flags | O_NONBLOCK);
  }
  else {
    fcntl(fileno(stdin), F_SETFL, save_flags);
  }
}

void
io_cooked_bang(void)
{
  struct termios settings;
  tcgetattr(fileno(stdin), &save_settings);
  settings = save_settings;
  settings.c_iflag |= (BRKINT | ISTRIP | IXON);
  settings.c_lflag |= (ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHONL);
  settings.c_cc[VMIN] = 1;
  settings.c_cc[VTIME] = 0;
  tcsetattr(fileno(stdin), TCSANOW, &settings);
  save_flags = fcntl(fileno(stdin), F_GETFL, 0);
  fcntl(fileno(stdin), F_SETFL, save_flags & ~O_NONBLOCK);
}

void
io_echo_eq(bool flag)
{
  struct termios settings;
  tcgetattr(fileno(stdin), &settings);
  if (flag) {
    settings.c_lflag |= ECHO;
  }
  else {
    settings.c_lflag &= ~ECHO;
  }
  tcsetattr(fileno(stdin), TCSANOW, &settings);
}

bool
io_echo_q(void)
{
  struct termios settings;
  tcgetattr(fileno(stdin), &settings);
  if (settings.c_lflag & ECHO) {
    return true;
  }
  else {
    return false;
  }
}

void
io__restore_termios(void)
{
  fcntl(fileno(stdin), F_SETFL, save_flags);
  tcsetattr(fileno(stdin), TCSANOW, &save_settings);
}
