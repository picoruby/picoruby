#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>


void
Net_sleep_ms(int ms)
{
  usleep(ms * 1000);
}

void
lwip_begin(void)
{
  // no-op
}

void
lwip_end(void)
{
  // no-op
}

int
mbedtls_hardware_poll(void *data __attribute__((unused)), unsigned char *output, size_t len, size_t *olen)
{
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1) {
    return -1;  // Error opening /dev/urandom
  }

  ssize_t bytes_read = read(fd, output, len);
  if (bytes_read < 0) {
    close(fd);
    return -1;  // Error reading from /dev/urandom
  }

  close(fd);

  // Set the number of bytes written to the output
  if (olen != NULL) {
    *olen = bytes_read;
  }

  return 0;  // Success
}
