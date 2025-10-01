#include <time.h>
#include <stdint.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>

typedef struct {
  struct timespec end_time;
  int             int_ms;
  int             fin_ms;
  int             active;
} timing_delay_context;

void
mbedtls_timing_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms)
{
  timing_delay_context *ctx = (timing_delay_context *)data;

  ctx->int_ms = (int)int_ms;
  ctx->fin_ms = (int)fin_ms;
  ctx->active = 1;

  if (fin_ms == 0) {
    // Cancel
    ctx->active = 0;
    return;
  }

  // current time + fin_ms = end_time
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  ctx->end_time.tv_sec = now.tv_sec + fin_ms / 1000;
  ctx->end_time.tv_nsec = now.tv_nsec + (fin_ms % 1000) * 1000000;
  if (ctx->end_time.tv_nsec >= 1000000000) {
    ctx->end_time.tv_sec += 1;
    ctx->end_time.tv_nsec -= 1000000000;
  }
}

int
mbedtls_timing_get_delay(void *data)
{
  timing_delay_context *ctx = (timing_delay_context *)data;

  if (ctx->fin_ms == 0 || !ctx->active) return -1;

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  int64_t elapsed_ms = (ctx->end_time.tv_sec - now.tv_sec) * 1000 +
                        (ctx->end_time.tv_nsec - now.tv_nsec) / 1000000;

  if (elapsed_ms <= -ctx->fin_ms) {
    return 2; // Final delay has passed
  } else if (elapsed_ms <= -ctx->int_ms) {
    return 1; // Intermediate delay has passed
  } else {
    return 0; // Still active, but no delay
  }
}

// Stub
unsigned long
mbedtls_timing_hardclock(void)
{
  static unsigned long counter = 0;
  return ++counter;
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
