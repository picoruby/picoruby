#ifndef IO_CONSOLE_DEFINED_H_
#define IO_CONSOLE_DEFINED_H_

#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

void c_raw_bang(mrbc_vm *vm, mrbc_value *v, int argc);
void c_cooked_bang(mrbc_vm *vm, mrbc_value *v, int argc);
void c__restore_termios(mrbc_vm *vm, mrbc_value *v, int argc);
void io_console_port_init(mrbc_vm *vm, mrbc_class *class_IO);

int hal_getchar(void);

#ifdef __cplusplus
}
#endif

#endif /* IO_CONSOLE_DEFINED_H_ */
