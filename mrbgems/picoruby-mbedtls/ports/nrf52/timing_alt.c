/*
 * nRF52 port: the two platform-specific pieces Mbed TLS needs -- a
 * millisecond delay timer for DTLS retransmission, and an entropy source.
 *
 * The timer reads the wall clock through gettimeofday, as the ESP32 port
 * does, rather than touching a timer peripheral. On this platform that
 * resolves to the product's clock shim, so the port improves for free
 * when the shim gains a real time base, and claims no hardware of its own.
 */

#include <stdint.h>
#include <stddef.h>
#include <sys/time.h>

#include "rng.h"

#include "../../include/timing_alt.h"

typedef struct {
  uint64_t start_ms;
  uint32_t int_ms;
  uint32_t fin_ms;
  int      active;
} timing_delay_context;

static uint64_t
now_ms(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000u + (uint64_t)(tv.tv_usec / 1000);
}

void
mbedtls_timing_set_delay(void *data, uint32_t int_ms, uint32_t fin_ms)
{
  timing_delay_context *ctx = (timing_delay_context *)data;

  ctx->int_ms = int_ms;
  ctx->fin_ms = fin_ms;

  if (fin_ms == 0) {
    ctx->active = 0;   /* cancel */
    return;
  }
  ctx->start_ms = now_ms();
  ctx->active = 1;
}

int
mbedtls_timing_get_delay(void *data)
{
  timing_delay_context *ctx = (timing_delay_context *)data;

  if (ctx->fin_ms == 0 || !ctx->active) {
    return -1;
  }

  /* Unsigned elapsed, so a clock that wraps still yields the right
     interval as long as the interval itself is shorter than the wrap.
     DTLS timeouts are seconds; the shortest wrap on this platform is
     several minutes. */
  uint64_t elapsed = now_ms() - ctx->start_ms;

  if (elapsed >= ctx->fin_ms) {
    return 2;
  }
  if (elapsed >= ctx->int_ms) {
    return 1;
  }
  return 0;
}

/*
 * Mbed TLS wants a monotonic tick for its own timing hardening. It does
 * not have to be a real cycle count, and tying it to a peripheral would
 * claim hardware for something that is only ever compared with itself.
 */
unsigned long
mbedtls_timing_hardclock(void)
{
  static unsigned long counter = 0;
  return ++counter;
}

/*
 * MBEDTLS_ENTROPY_HARDWARE_ALT is on, so this is the only entropy the
 * library gets. It comes from the nRF52 RNG peripheral through
 * picoruby-rng, which applies the bias correction the raw stream needs.
 */
int
mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
  (void)data;

  for (size_t i = 0; i < len; i++) {
    output[i] = rng_random_byte_impl();
  }
  if (olen != NULL) {
    *olen = len;
  }
  return 0;
}
