#ifndef IO_CONSOLE_DEFINED_H_
#define IO_CONSOLE_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

int hal_getchar(void);
int hal_write(int fd, const void *buf, int nbytes);

bool io_raw_q(void);
void io_raw_bang(bool nonblock);
void io_cooked_bang(void);
void io_echo_eq(bool flag);
bool io_echo_q(void);
void io__restore_termios(void);

#ifdef __cplusplus
}
#endif

#endif /* IO_CONSOLE_DEFINED_H_ */
