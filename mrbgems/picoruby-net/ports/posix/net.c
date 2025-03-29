#include "mbedtls/entropy.h"
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int
mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
  int fd;
  size_t read_len;
  (void)data;
  *olen = 0;
  if ((fd = open("/dev/urandom", O_RDONLY)) < 0) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  read_len = read(fd, output, len);
  if (read_len <= 0) {
    close(fd);
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  *olen = read_len;
  if (close(fd) < 0) {
    return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
  }
  return 0;
}

int
sleep_ms(long milliseconds)
{
  struct timespec req, rem;
  int ret;
  req.tv_sec = milliseconds / 1000;
  req.tv_nsec = (milliseconds % 1000) * 1000000L;
  while ((ret = nanosleep(&req, &rem)) == -1 && errno == EINTR) {
    req = rem;
  }
  return ret;
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
