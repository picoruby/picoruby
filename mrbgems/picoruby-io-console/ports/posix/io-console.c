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
  settings.c_iflag = ~(BRKINT | ISTRIP | IXON);
  settings.c_lflag = ~(ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHONL);
  settings.c_cc[VMIN]  = 1;
  settings.c_cc[VTIME] = 0;
  tcsetattr(fileno(stdin), TCSANOW, &settings);
  save_flags = fcntl(fileno(stdin), F_GETFL, 0);
  fcntl(fileno(stdin), F_SETFL, save_flags | O_NONBLOCK); /* add `non blocking` */
  SET_RETURN(v[0]);
}

void
c_cooked_bang(mrbc_vm *vm, mrbc_value *v, int argc)
{
  fcntl(fileno(stdin), F_SETFL, save_flags);
  tcsetattr(fileno(stdin), TCSANOW, &save_settings);
  SET_RETURN(v[0]);
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

void
io_console_port_init(mrbc_vm *vm, mrbc_class *class_IO)
{
  mrbc_define_method(vm, class_IO, "raw!", c_raw_bang);
  mrbc_define_method(vm, class_IO, "cooked!", c_cooked_bang);
}

