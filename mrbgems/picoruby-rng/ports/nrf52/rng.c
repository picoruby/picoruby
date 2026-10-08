/*
 * nRF52 port: the on-chip RNG, which derives entropy from thermal
 * noise rather than a PRNG seed.
 *
 * Bias correction (DERCEN) is left on. It costs time -- the datasheet
 * puts a corrected byte at roughly 120 us against 30 us raw -- but the
 * raw stream is documented as biased, and a caller asking for a random
 * byte is not expecting to whiten it themselves. Blocking that long is
 * acceptable here because rng_random_byte_impl() is a byte-at-a-time
 * synchronous contract; a caller pulling a key's worth should be doing
 * it through something buffered, not this.
 */

#include <stdint.h>

#include "nrf.h"
#include "nrf_rng.h"

#include "../../include/rng.h"

/* Iterations, not microseconds: the loop body is a register read and the
   compiler is free to schedule it, so this errs long -- the safe
   direction for a backstop that must never fire on healthy hardware. */
#define RNG_SPIN_BUDGET 10000000u

uint8_t
rng_random_byte_impl(void)
{
  uint8_t value;

  nrf_rng_error_correction_enable();
  nrf_rng_event_clear(NRF_RNG_EVENT_VALRDY);
  nrf_rng_task_trigger(NRF_RNG_TASK_START);

  /*
   * Bounded. nrfx_rng is not an option here -- nrfx_rng_init asserts on a
   * NULL handler, so the driver has no blocking mode and this gem's
   * contract is a synchronous byte -- which leaves the wait to this port,
   * and an unbounded one would hand a single stuck peripheral the whole
   * VM. A corrected byte is about 120 us; the budget is four orders of
   * magnitude past that, so it costs nothing on a working part and still
   * ends on a dead one.
   */
  uint32_t budget = RNG_SPIN_BUDGET;
  while (!nrf_rng_event_get(NRF_RNG_EVENT_VALRDY)) {
    if (budget-- == 0) {
      nrf_rng_task_trigger(NRF_RNG_TASK_STOP);
      return 0;
    }
  }

  value = nrf_rng_random_value_get();
  nrf_rng_event_clear(NRF_RNG_EVENT_VALRDY);

  /* Stopped rather than left running: an idle RNG keeps its oscillator
     and drops the core out of the deepest sleep the board can reach. */
  nrf_rng_task_trigger(NRF_RNG_TASK_STOP);

  return value;
}
