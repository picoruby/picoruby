#include "pico/time.h"
#include "mbedtls/timing.h"

typedef struct {
  absolute_time_t end_time;
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
    // Cancel the timer
    ctx->active = 0;
    return;
  }

  // Set end time to now + fin_ms
  ctx->end_time = make_timeout_time_ms(fin_ms);
}

int
mbedtls_timing_get_delay(void *data)
{
  timing_delay_context *ctx = (timing_delay_context *)data;

  if (ctx->fin_ms == 0 || !ctx->active)
    return -1;

  absolute_time_t now = get_absolute_time();
  int64_t elapsed_ms = absolute_time_diff_us(now, ctx->end_time) / 1000;

  if (elapsed_ms <= -ctx->fin_ms) {
    return 2; // Final delay passed
  } else if (elapsed_ms <= -ctx->int_ms) {
    return 1; // Intermediate delay passed
  } else {
    return 0; // No delay passed yet
  }
}

// Stub function for mbedtls_timing_hardclock
unsigned long
mbedtls_timing_hardclock(void)
{
  static unsigned long counter = 0;
  return ++counter;
}
