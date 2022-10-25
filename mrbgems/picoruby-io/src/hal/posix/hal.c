#include <stdio.h>
#include <termios.h>
#include <fcntl.h>

#include <mrubyc.h>

#include "../hal.h"

static int sigint;

void
setGlobalSigint(int i)
{
  sigint = i;
}

int
globalSigint(void)
{
  return sigint;
}

int
hal_read_char(int fd)
{
  struct termios save_settings;
  struct termios settings;
  tcgetattr(fileno(stdin), &save_settings);
  settings = save_settings;
  settings.c_lflag &= ~(ECHO | ICANON); /* no echoback & no wait for LF */
  tcsetattr(fileno(stdin), TCSANOW, &settings);
  fcntl(fileno(stdin), F_SETFL, O_NONBLOCK); /* non blocking */
  int ch = getchar();
  for (;;) {
    ch = getchar();
    if (ch != EOF) break;
    //if (sigint) {
    if (globalSigint()) {
      ch = 3;
      //sigint = 0;
      setGlobalSigint(0);
      break;
    }
  }
  tcsetattr(fileno(stdin), TCSANOW, &save_settings);
  return ch;
}

int
hal_read_nonblock(int fd, char *buf, int nbytes)
{
  struct termios save_settings;
  struct termios settings;
  {
    tcgetattr(fileno(stdin), &save_settings);
    settings = save_settings;
    settings.c_lflag &= ~(ECHO | ICANON); /* no echoback & no wait for LF */
    tcsetattr(fileno(stdin), TCSANOW, &settings);
    fcntl(fileno(stdin), F_SETFL, O_NONBLOCK); /* non blocking */
  }
  int c;
  size_t len;
  for(len = 0; len < nbytes; len++) {
    c = getchar();
    if ( c == EOF ) {
      break;
    } else {
      buf[len] = c;
    }
  }
  buf[len] = '\0';
  {
    tcsetattr(fileno( stdin ), TCSANOW, &save_settings);
  }
  return (int)len;
}

int
hal_read_available(int fd)
{
  //FIXME
  return 1;
}

