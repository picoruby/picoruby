/*
 * nRF52 port: the on-chip RNG, which derives entropy from thermal
 * noise rather than a PRNG seed.
 *
 * Bias correction (DERCEN) is left on. It costs time -- the datasheet
 * puts a corrected byte at roughly 120 us against 30 us raw -- but the
 * raw stream is documented as biased, and a caller asking for random
 * bytes is not expecting to whiten them itself.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nrf.h"
#include "nrf_rng.h"

#include "../../include/rng.h"

/* Iterations, not microseconds: the loop body is a register read and the
   compiler is free to schedule it, so this errs long -- the safe
   direction for a backstop that must never fire on healthy hardware. A
   corrected byte is about 120 us and this is four orders of magnitude
   past that. */
#define RNG_SPIN_BUDGET 10000000u

int
rng_random_bytes_impl(uint8_t *buf, size_t len)
{
  bool irq_was_enabled;
  int result = 0;

  if (buf == NULL) {
    return -1;
  }
  if (len == 0) {
    return 0;
  }

  /*
   * EVENTS_VALRDY is a single latch, not a counter, so an
   * interrupt-driven consumer of the same peripheral can read the value
   * and clear the event between our START and our poll -- OpenThread's
   * otPlatEntropyGet is one, and it owns RNG_IRQHandler when linked.
   * This loop would then spin out its budget on healthy hardware.
   * Masking the vector for the duration keeps the event ours. Nothing
   * here masks interrupts globally, so the scheduler tick is unaffected.
   */
  irq_was_enabled = (NVIC_GetEnableIRQ(RNG_IRQn) != 0);
  if (irq_was_enabled) {
    NVIC_DisableIRQ(RNG_IRQn);
  }

  nrf_rng_error_correction_enable();
  nrf_rng_event_clear(NRF_RNG_EVENT_VALRDY);
  /* Started once for the whole buffer: the generator keeps running and
     raises VALRDY per byte, so a START per byte would pay the start-up
     latency len times over. */
  nrf_rng_task_trigger(NRF_RNG_TASK_START);

  for (size_t i = 0; i < len; i++) {
    uint32_t budget = RNG_SPIN_BUDGET;

    while (!nrf_rng_event_get(NRF_RNG_EVENT_VALRDY)) {
      if (budget-- == 0) {
        /*
         * Report it rather than filling the rest with zeros. A caller
         * seeding a DRBG cannot tell fabricated entropy from real after
         * the fact, so this is the only place the difference can be
         * stated.
         */
        result = -1;
        goto done;
      }
    }
    buf[i] = nrf_rng_random_value_get();
    nrf_rng_event_clear(NRF_RNG_EVENT_VALRDY);
  }

done:
  /* Stopped rather than left running: an idle RNG keeps its oscillator
     and drops the core out of the deepest sleep the board can reach. */
  nrf_rng_task_trigger(NRF_RNG_TASK_STOP);
  if (irq_was_enabled) {
    NVIC_EnableIRQ(RNG_IRQn);
  }
  return result;
}

/*
 * The byte-at-a-time contract, which cannot report a failure. Callers
 * that must not proceed on fabricated entropy use the call above; this
 * one yields 0 on a source that never became ready, which is
 * indistinguishable from a legitimate 0x00 and is why it is not the
 * interface the entropy path uses.
 */
uint8_t
rng_random_byte_impl(void)
{
  uint8_t value = 0;

  (void)rng_random_bytes_impl(&value, 1);
  return value;
}
