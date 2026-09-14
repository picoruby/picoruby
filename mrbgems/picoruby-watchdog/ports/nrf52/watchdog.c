/*
 * nRF52 port.
 *
 * The WDT here runs off the 32.768 kHz LFCLK and, once started, cannot
 * be stopped or reconfigured until the next reset -- that is the whole
 * point of the peripheral on this part, not an omission in this file.
 * Watchdog_disable and Watchdog_start_tick say so below rather than
 * pretending to work.
 */
#include <stdbool.h>
#include <stdint.h>

#include "nrf.h"
#include "nrf_wdt.h"

#include "machine.h"

/* Include the gem's own contract so the definitions below are checked
   against it, rather than only being discovered at link time. */
#include "../../include/watchdog.h"

/* The WDT counts LFCLK ticks, so a reload value is a time in 32768ths
   of a second, less one. */
#define WDT_TICKS_PER_SECOND 32768u
/* CRV bottoms out at 0xF, and CRV == ticks - 1. */
#define WDT_TICKS_MIN        16u

/*
 * RESETREAS accumulates: a bit stays set through subsequent resets
 * until software clears it, so a DOG bit from three boots ago would
 * still read as "the watchdog just rebooted us". Latch it once on
 * first use and clear what we saw, which is the pattern Nordic's own
 * examples use.
 */
static uint32_t reset_reason;
static bool     reset_reason_latched;

static uint32_t
latched_reset_reason(void)
{
  if (!reset_reason_latched) {
    reset_reason = NRF_POWER->RESETREAS;
    NRF_POWER->RESETREAS = reset_reason;  /* write-one-to-clear */
    reset_reason_latched = true;
  }
  return reset_reason;
}

void
Watchdog_enable(uint32_t delay_ms, bool pause_on_debug)
{
  /* CRV, RREN and CONFIG are ignored once the WDT is running, so a
     second call would silently keep the first timeout. Return instead:
     the caller asked for something this hardware cannot give. */
  if (nrf_wdt_started()) {
    return;
  }

  /*
   * Take the RESETREAS snapshot here as well as in the predicates.
   * A program that asks "did the watchdog reboot us?" is a program that
   * also enables the watchdog, and this is the call it makes first, so
   * the snapshot ends up describing the boot the caller is asking
   * about. It is still lazy -- a build that never touches Watchdog at
   * all leaves RESETREAS uncleared for the next boot -- but nothing is
   * misled by bits nobody reads.
   */
  (void)latched_reset_reason();

  uint64_t ticks = ((uint64_t)delay_ms * WDT_TICKS_PER_SECOND) / 1000u;
  if (ticks > UINT32_MAX) {
    ticks = UINT32_MAX;
  }
  /*
   * Refuse rather than clamp. CRV is specified over [0xF, 0xFFFFFFFF],
   * and delay_ms == 0 would otherwise reach CRV = 0 -- roughly a 30 us
   * timeout. Started from a boot script that resets the board before it
   * can reach a prompt, and the WDT cannot be stopped, so on a
   * USB-CDC-only board there is no way back in. Anything under half a
   * millisecond is a caller mistake, not a watchdog.
   */
  if (ticks < WDT_TICKS_MIN) {
    return;
  }
  nrf_wdt_reload_value_set((uint32_t)(ticks - 1));

  /* Keep counting through SLEEP either way -- the VM idles in __WFE
     and would otherwise never be watched. HALT is the debugger case:
     pausing there is what pause_on_debug asks for. */
  nrf_wdt_behaviour_set(pause_on_debug ? NRF_WDT_BEHAVIOUR_RUN_SLEEP
                                       : NRF_WDT_BEHAVIOUR_RUN_SLEEP_HALT);

  nrf_wdt_reload_request_enable(NRF_WDT_RR0);
  nrf_wdt_task_trigger(NRF_WDT_TASK_START);
}

void
Watchdog_disable(void)
{
  /* Not supported: the nRF52 WDT has no stop task, and clearing
   * CONFIG/RREN while it runs has no effect. Once enabled it must be
   * fed until reset. */
}

void
Watchdog_reboot(uint32_t delay_ms)
{
  /*
   * Feed while waiting. Without this the dog beats us to the reset
   * whenever delay_ms exceeds the configured timeout, RESETREAS records
   * DOG instead of SREQ, and Watchdog.enable_caused_reboot? then blames
   * a timeout for what the caller explicitly asked for. Stepping in 10 ms
   * slices still loses to a timeout shorter than that, which is a
   * watchdog too tight to survive any reboot delay at all.
   */
  while (delay_ms > 0) {
    uint32_t step = (delay_ms > 10u) ? 10u : delay_ms;
    Watchdog_update();
    Machine_delay_ms(step);
    delay_ms -= step;
  }
  Machine_reboot();
}

void
Watchdog_start_tick(uint32_t cycles)
{
  /* Not supported, and nothing to support: the RP2040 tick divider
   * exists because its watchdog counts a 12 MHz reference. The nRF52
   * WDT is wired to the 32.768 kHz LFCLK with no prescaler. */
  (void)cycles;
}

void
Watchdog_update(void)
{
  if (nrf_wdt_started()) {
    nrf_wdt_reload_request_set(NRF_WDT_RR0);
  }
}

bool
Watchdog_caused_reboot(void)
{
  return (latched_reset_reason() & POWER_RESETREAS_DOG_Msk) != 0;
}

bool
Watchdog_enable_caused_reboot(void)
{
  /*
   * Same answer as Watchdog_caused_reboot, and that is correct here
   * rather than lazy. RP2040 needs the two apart because its
   * watchdog_reboot() also trips the watchdog; on nRF52 a forced
   * reboot goes through NVIC_SystemReset and sets SREQ, so DOG can
   * only mean a timeout that an enabled watchdog was not fed through.
   */
  return (latched_reset_reason() & POWER_RESETREAS_DOG_Msk) != 0;
}

uint32_t
Watchdog_get_count(void)
{
  /* Not supported: the nRF52 WDT exposes CRV and the reload requests,
   * but never the live down-counter, so there is no remaining time to
   * report. */
  return 0;
}
