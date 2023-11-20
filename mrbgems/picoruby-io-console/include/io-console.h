#ifndef IO_CONSOLE_DEFINED_H_
#define IO_CONSOLE_DEFINED_H_

#include <mrubyc.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MRBC_USE_HAL_POSIX
void c_raw_bang(mrbc_vm *vm, mrbc_value *v, int argc);
void c_cooked_bang(mrbc_vm *vm, mrbc_value *v, int argc);
void io_console_port_init(void);
#endif

int hal_getchar(void);

#ifdef __cplusplus
}
#endif

#endif /* IO_CONSOLE_DEFINED_H_ */
