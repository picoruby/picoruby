#include <stdio.h>
#include <termios.h>
#include <fcntl.h>

#include <mrubyc.h>

static struct termios save_settings;
static int save_flags;

void
c_raw_bang(mrbc_vm *vm, mrbc_value *v, int argc)
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
  if (0 < argc) {
    fcntl(fileno(stdin), F_SETFL, save_flags | O_NONBLOCK); /* add `non blocking` */
  }
  else {
    fcntl(fileno(stdin), F_SETFL, save_flags);
  }
}
void
c_cooked_bang(mrbc_vm *vm, mrbc_value *v, int argc)
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
c__restore_termios(mrbc_vm *vm, mrbc_value *v, int argc)
{
  fcntl(fileno(stdin), F_SETFL, save_flags);
  tcsetattr(fileno(stdin), TCSANOW, &save_settings);
}

int
hal_getchar(void)
{
  int c = getchar();
  if (c == EOF) {
    return -1;
  } else {
    return c;
  }
}

static void
c_echo_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  struct termios settings;
  tcgetattr(fileno(stdin), &settings);
  if (v[1].tt == MRBC_TT_TRUE) {
    settings.c_lflag |= ECHO;
  }
  else {
    settings.c_lflag &= ~ECHO;
  }
  tcsetattr(fileno(stdin), TCSANOW, &settings);
//  mrbc_incref(&v[0]);
  SET_RETURN(v[1]);
}

static void
c_echo_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  struct termios settings;
  tcgetattr(fileno(stdin), &settings);
  if (settings.c_lflag & ECHO) {
    SET_TRUE_RETURN();
  }
  else {
    SET_FALSE_RETURN();
  }
}

void
io_console_port_init(mrbc_vm *vm, mrbc_class *class_IO)
{
  mrbc_define_method(vm, class_IO, "raw!", c_raw_bang);
  mrbc_define_method(vm, class_IO, "cooked!", c_cooked_bang);
  mrbc_define_method(vm, class_IO, "_restore_termios", c__restore_termios);

  mrbc_define_method(vm, class_IO, "echo=", c_echo_eq);
  mrbc_define_method(vm, class_IO, "echo?", c_echo_q);
}

