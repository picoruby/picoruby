#ifndef IO_HAL_H_
#define IO_HAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MRBC_USE_HAL_POSIX
void setGlobalSigint(int i);
int globalSigint(void);
#endif

#define STDIN_FD  0
#define STDOUT_FD 1

int hal_read_char(int fd);
int hal_read_nonblock(int fd, char *buf, int nbytes);
int hal_read_available(int fd);

#ifdef __cplusplus
}
#endif

#endif /* IO_HAL_H_ */
