/*
 * nRF52 port: GPIO interrupts on GPIOTE IN channels.
 *
 * This talks to the GPIOTE HAL (nrf_gpiote.h, header-only) and owns
 * GPIOTE_IRQHandler directly rather than going through nrfx_gpiote.
 * The driver would pull nrfx_gpiote.c into the firmware and require
 * NRFX_GPIOTE_ENABLED in the product's sdk_config.h, which no nRF52
 * product enables today; the HAL needs neither, so a port PR stays a
 * port PR. A build that later wants nrfx_gpiote must take the vector
 * back from here -- both cannot define GPIOTE_IRQHandler.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_rtc.h"

#include "../../include/irq.h"

/* One handler per GPIOTE IN channel: the channel is the scarce
   resource here, not the table. */
#define IRQ_MAX_HANDLERS     GPIOTE_CH_NUM
#define IRQ_EVENT_QUEUE_SIZE (1 << 5)
/* Same as NRFX_GPIOTE_CONFIG_IRQ_PRIORITY, app_timer and nrfx_usbd. */
#define IRQ_GPIOTE_NVIC_PRIORITY 6

typedef struct {
  int      pin;
  uint32_t event_mask;         /* picorb_gpio_irq_event_t bits */
  bool     enabled;
  uint32_t debounce_ticks;     /* 0 disables debouncing */
  uint32_t last_event_ticks;
  uint32_t last_event_type;
} irq_handler_t;

typedef struct {
  int irq_id;
  int event_type;
} irq_event_t;

static irq_handler_t irq_handlers[IRQ_MAX_HANDLERS];
static irq_event_t   event_queue[IRQ_EVENT_QUEUE_SIZE];
static volatile int  queue_head = 0;
static volatile int  queue_tail = 0;

static const nrf_gpiote_events_t in_event[IRQ_MAX_HANDLERS] = {
  NRF_GPIOTE_EVENTS_IN_0, NRF_GPIOTE_EVENTS_IN_1,
  NRF_GPIOTE_EVENTS_IN_2, NRF_GPIOTE_EVENTS_IN_3,
  NRF_GPIOTE_EVENTS_IN_4, NRF_GPIOTE_EVENTS_IN_5,
  NRF_GPIOTE_EVENTS_IN_6, NRF_GPIOTE_EVENTS_IN_7,
};

/*
 * Debounce timebase: RTC1's free-running counter.
 *
 * The obvious choice, DWT->CYCCNT, is wrong here. It counts processor
 * clock cycles, and this VM idles in __WFE on essentially every
 * scheduler pass, so the counter stops whenever the board is doing
 * nothing -- which is exactly when a button press arrives. A 50 ms
 * debounce would become 50 ms of CPU-active time, seconds of wall clock
 * on an idle board, and presses would be dropped unpredictably.
 *
 * RTC1 runs from the 32.768 kHz LFCLK and keeps counting through WFE.
 * It is app_timer's timer on this platform, but reading COUNTER takes
 * nothing from it: no compare channel, no interrupt, one load.
 *
 * If nothing has started RTC1 the counter reads a constant, and the
 * "no time has passed" case below deliberately lets the event through.
 * Debouncing then does nothing, which is the right way to fail: the
 * alternative -- treating a frozen clock as "too soon" -- would kill
 * the pin permanently and give no clue why.
 */
#define IRQ_RTC_TICKS_PER_SECOND 32768u
#define IRQ_RTC_COUNTER_MASK     RTC_COUNTER_COUNTER_Msk  /* 24-bit */

static uint32_t
ticks_now(void)
{
  return nrf_rtc_counter_get(NRF_RTC1) & IRQ_RTC_COUNTER_MASK;
}

static uint32_t
ms_to_ticks(uint32_t ms)
{
  uint64_t ticks = ((uint64_t)ms * IRQ_RTC_TICKS_PER_SECOND) / 1000u;

  /* The counter is 24 bits, so it wraps every 512 s and an interval
     longer than that cannot be measured. Cap at half the range, past
     which a "debounce" is a timer the caller should own. */
  if (ticks > (IRQ_RTC_COUNTER_MASK / 2)) {
    ticks = IRQ_RTC_COUNTER_MASK / 2;
  }
  return (uint32_t)ticks;
}

/* GPIOTE IN detects transitions only. A level request is rejected in
   IRQ_register_gpio, so the polarity below is always an edge. */
static nrf_gpiote_polarity_t
polarity_from_event_mask(uint32_t event_mask)
{
  bool rise = (event_mask & PICORB_GPIO_IRQ_EVENT_EDGE_RISE) != 0;
  bool fall = (event_mask & PICORB_GPIO_IRQ_EVENT_EDGE_FALL) != 0;

  if (rise && fall) {
    return NRF_GPIOTE_POLARITY_TOGGLE;
  }
  return rise ? NRF_GPIOTE_POLARITY_LOTOHI : NRF_GPIOTE_POLARITY_HITOLO;
}

/*
 * Which edge actually fired. GPIOTE does not record it, so for a
 * both-edges registration the pin is sampled instead -- and that is a
 * guess, not a reading: the sample happens an interrupt latency after
 * the edge, so a pulse shorter than that has already returned and is
 * reported as the opposite edge. Fine for buttons and slow lines, not
 * for encoders or IR bursts. Recovering the true edge would need two
 * GPIOTE channels per registration, one per polarity.
 */
static int
event_type_of(const irq_handler_t *handler)
{
  uint32_t mask = handler->event_mask;

  if ((mask & PICORB_GPIO_IRQ_EVENT_EDGE_RISE) &&
      (mask & PICORB_GPIO_IRQ_EVENT_EDGE_FALL)) {
    return nrf_gpio_pin_read((uint32_t)handler->pin)
             ? PICORB_GPIO_IRQ_EVENT_EDGE_RISE
             : PICORB_GPIO_IRQ_EVENT_EDGE_FALL;
  }
  return (int)mask;
}

void
GPIOTE_IRQHandler(void)
{
  uint32_t now = ticks_now();

  for (int ch = 0; ch < IRQ_MAX_HANDLERS; ch++) {
    /* Check INTEN too, as Nordic's handler does: clearing an event on a
       channel this gem never allocated would destroy it for whoever did
       (a PPI consumer, say). */
    if (!nrf_gpiote_event_is_set(in_event[ch]) ||
        !nrf_gpiote_int_is_enabled(NRF_GPIOTE_INT_IN0_MASK << ch)) {
      continue;
    }
    nrf_gpiote_event_clear(in_event[ch]);

    irq_handler_t *handler = &irq_handlers[ch];
    if (!handler->enabled) {
      continue;
    }

    int event_type = event_type_of(handler);

    uint32_t elapsed = (now - handler->last_event_ticks) & IRQ_RTC_COUNTER_MASK;
    if (handler->debounce_ticks > 0 && elapsed > 0 &&
        elapsed < handler->debounce_ticks &&
        (uint32_t)event_type == handler->last_event_type) {
      continue;
    }
    handler->last_event_ticks = now;
    handler->last_event_type = (uint32_t)event_type;

    int next_tail = (queue_tail + 1) & (IRQ_EVENT_QUEUE_SIZE - 1);
    if (next_tail != queue_head) {
      event_queue[queue_tail].irq_id = ch + 1;  /* IRQ IDs are 1-based */
      event_queue[queue_tail].event_type = event_type;
      queue_tail = next_tail;
#if defined(PICORB_IRQ_EVENT_BRIDGE)
      /* Publish only after the event is visible in the queue, so a task
         woken by the token always finds it. A dropped event (queue full)
         does not signal: there is nothing new to drain. */
      IRQ_signal_from_isr(IRQ_SRC_GPIO, 1);
#endif
    }
  }
}

int
IRQ_register_gpio(int pin, int event_type, uint32_t debounce_ms)
{
  /*
   * Level-triggered requests are refused rather than approximated.
   * GPIOTE IN is edge-only; level detection on nRF52 runs through
   * PIN_CNF SENSE and the shared PORT event, whose DETECT signal stays
   * asserted for as long as the level holds. Re-arming it on the
   * opposite polarity after each event -- the usual trick -- turns it
   * back into edge detection, so a caller asking for LEVEL_HIGH would
   * silently get EDGE_RISE. Better to fail here than to lie.
   */
  if (event_type & (PICORB_GPIO_IRQ_EVENT_LEVEL_LOW |
                    PICORB_GPIO_IRQ_EVENT_LEVEL_HIGH)) {
    return -1;
  }
  if ((event_type & (PICORB_GPIO_IRQ_EVENT_EDGE_RISE |
                     PICORB_GPIO_IRQ_EVENT_EDGE_FALL)) == 0) {
    return -1;
  }
  if (pin < 0 || (uint32_t)pin >= NUMBER_OF_PINS) {
    return -1;
  }

  int slot = -1;
  for (int i = 0; i < IRQ_MAX_HANDLERS; i++) {
    if (!irq_handlers[i].enabled) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    return -1;  /* every GPIOTE channel is taken */
  }

  uint32_t event_mask = (uint32_t)event_type &
    (PICORB_GPIO_IRQ_EVENT_EDGE_RISE | PICORB_GPIO_IRQ_EVENT_EDGE_FALL);

  /*
   * Connect the input buffer first. GPIOTE overrides DIR for the pin it
   * watches, but nothing in the datasheet says it touches PIN_CNF.INPUT,
   * and Nordic's own driver does not assume it does: nrfx_gpiote calls
   * nrf_gpio_cfg_input/cfg_watcher before configuring the event, and
   * skips it only when the caller declares it already did the setup.
   * nrf_gpio_pin_read is documented as valid only with the input buffer
   * connected, and event_type_of() below depends on it.
   *
   * cfg_watcher is a read-modify-write of INPUT alone, so whatever
   * direction, pull and drive picoruby-gpio configured survive. Note
   * that watching a pin does stop it driving, so a GPIO::OUT pin goes
   * high-impedance for as long as the IRQ is registered -- unlike
   * RP2040, where an output can carry an interrupt.
   */
  nrf_gpio_cfg_watcher((uint32_t)pin);

  nrf_gpiote_event_configure((uint32_t)slot, (uint32_t)pin,
                             polarity_from_event_mask(event_mask));
  nrf_gpiote_event_clear(in_event[slot]);
  nrf_gpiote_event_enable((uint32_t)slot);

  irq_handlers[slot].pin = pin;
  irq_handlers[slot].event_mask = event_mask;
  irq_handlers[slot].debounce_ticks = ms_to_ticks(debounce_ms);
  irq_handlers[slot].last_event_ticks = ticks_now();
  irq_handlers[slot].last_event_type = 0;
  irq_handlers[slot].enabled = true;

  nrf_gpiote_int_enable(NRF_GPIOTE_INT_IN0_MASK << slot);
  /*
   * Priority matters: NVIC IPR resets to 0, the highest on the part, and
   * this handler would then preempt the USB device ISR and the RTC1 tick
   * -- both of which the SDK explicitly puts at 6. Match them.
   */
  NVIC_SetPriority(GPIOTE_IRQn, IRQ_GPIOTE_NVIC_PRIORITY);
  NVIC_ClearPendingIRQ(GPIOTE_IRQn);
  NVIC_EnableIRQ(GPIOTE_IRQn);

  return slot + 1;
}

bool
IRQ_unregister_gpio(int irq_id)
{
  int slot = irq_id - 1;

  if (slot < 0 || slot >= IRQ_MAX_HANDLERS || !irq_handlers[slot].enabled) {
    return false;
  }

  nrf_gpiote_int_disable(NRF_GPIOTE_INT_IN0_MASK << slot);
  nrf_gpiote_event_disable((uint32_t)slot);
  nrf_gpiote_event_clear(in_event[slot]);
  memset(&irq_handlers[slot], 0, sizeof(irq_handler_t));

  return true;
}

bool
IRQ_peek_event(int *irq_id, int *event_type)
{
  if (queue_head == queue_tail) {
    return false;
  }

  *irq_id = event_queue[queue_head].irq_id;
  *event_type = event_queue[queue_head].event_type;
  queue_head = (queue_head + 1) & (IRQ_EVENT_QUEUE_SIZE - 1);

  return true;
}

void
IRQ_init(void)
{
  for (int i = 0; i < IRQ_MAX_HANDLERS; i++) {
    if (irq_handlers[i].enabled) {
      nrf_gpiote_int_disable(NRF_GPIOTE_INT_IN0_MASK << i);
      nrf_gpiote_event_disable((uint32_t)i);
    }
    nrf_gpiote_event_clear(in_event[i]);
  }
  memset(irq_handlers, 0, sizeof(irq_handlers));
  memset(event_queue, 0, sizeof(event_queue));
  queue_head = 0;
  queue_tail = 0;
}

#if defined(PICORB_IRQ_EVENT_BRIDGE)

/*
 * Cortex-M4 has LDREX/STREX, so the GCC builtins lower to inline
 * exclusive sequences -- no libatomic, and no interrupt masking of the
 * kind the Cortex-M0+ rp2040 port needs.
 */

uint32_t
IRQ_hal_atomic_load_u32(const volatile uint32_t *p)
{
  return __atomic_load_n(p, __ATOMIC_SEQ_CST);
}

uint32_t
IRQ_hal_atomic_or_u32(volatile uint32_t *p, uint32_t bits)
{
  return __atomic_fetch_or(p, bits, __ATOMIC_SEQ_CST);
}

uint32_t
IRQ_hal_atomic_and_u32(volatile uint32_t *p, uint32_t mask)
{
  return __atomic_fetch_and(p, mask, __ATOMIC_SEQ_CST);
}

uint32_t
IRQ_hal_atomic_exchange_u32(volatile uint32_t *p, uint32_t v)
{
  return __atomic_exchange_n(p, v, __ATOMIC_SEQ_CST);
}

#endif /* PICORB_IRQ_EVENT_BRIDGE */
