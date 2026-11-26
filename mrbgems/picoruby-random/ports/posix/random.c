#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../include/picorb_random.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

/*
 * Reads /dev/urandom. Emscripten fills its /dev/urandom from
 * crypto.getRandomValues.
 */
int
picorb_random_bytes(void *buf, size_t len)
{
  unsigned char *p = (unsigned char *)buf;
  int fd;

  do {
    fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) return -1;

  while (0 < len) {
    ssize_t n = read(fd, p, len);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) {
      close(fd);
      return -1;
    }
    p += n;
    len -= (size_t)n;
  }
  close(fd);
  return 0;
}
