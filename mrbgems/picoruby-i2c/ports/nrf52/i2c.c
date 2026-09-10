/*
 * nRF52 port: TWIM0 and TWIM1, EasyDMA, blocking transfers.
 *
 * TWIM shares its hardware with SPIM and TWIS of the same index -- TWIM0
 * with SPIM0/TWIS0, and so on -- so a board can have I2C or SPI on unit
 * 0, not both. Nothing here can enforce that across gems; the SDK's PRS
 * arbitration only covers drivers that opt into it, and this port talks
 * to the HAL directly. Documented in the gem README instead.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_twim.h"

#include "../../include/i2c.h"

#define I2C_UNIT_COUNT TWIM_COUNT

/* nRF52840 Data RAM is 0x20000000..0x2003FFFF. */
#define IS_IN_RAM(p) (((uintptr_t)(p) & 0xE0000000u) == 0x20000000u)
/* Long enough for the STOP condition itself, short enough that a stuck
   bus does not double the caller's timeout. */
#define STOP_TIMEOUT_US 1000u

static NRF_TWIM_Type *const instances[I2C_UNIT_COUNT] = { NRF_TWIM0, NRF_TWIM1 };

typedef struct {
  bool     initialized;
  int8_t   sda_pin;
  int8_t   scl_pin;
} i2c_unit_t;

static i2c_unit_t units[I2C_UNIT_COUNT];

int
I2C_unit_name_to_unit_num(const char *unit_name)
{
  if (strcmp(unit_name, "NRF52_I2C0") == 0 ||
      strcmp(unit_name, "NRF52_TWIM0") == 0) {
    return 0;
  }
  if (strcmp(unit_name, "NRF52_I2C1") == 0 ||
      strcmp(unit_name, "NRF52_TWIM1") == 0) {
    return 1;
  }
  return I2C_ERROR_INVALID_UNIT;
}

/*
 * TWIM offers three fixed bit rates and nothing between them. Round the
 * request down to one the hardware can actually produce rather than up:
 * a device rated for 100 kHz must not be driven at 250 kHz because the
 * caller asked for 150 kHz.
 */
static nrf_twim_frequency_t
frequency_from_hz(uint32_t hz)
{
  if (hz >= 400000u) {
    return NRF_TWIM_FREQ_400K;
  }
  if (hz >= 250000u) {
    return NRF_TWIM_FREQ_250K;
  }
  return NRF_TWIM_FREQ_100K;
}

i2c_status_t
I2C_gpio_init(int unit_num, uint32_t frequency, int8_t sda_pin, int8_t scl_pin)
{
  if (unit_num < 0 || I2C_UNIT_COUNT <= unit_num) {
    return I2C_ERROR_INVALID_UNIT;
  }
  if (sda_pin < 0 || scl_pin < 0 ||
      (uint32_t)sda_pin >= NUMBER_OF_PINS || (uint32_t)scl_pin >= NUMBER_OF_PINS) {
    return I2C_ERROR_UNDETERMINED;
  }

  NRF_TWIM_Type *twim = instances[unit_num];

  /* Reconfiguring a live unit: stop driving before the pins move. */
  if (units[unit_num].initialized) {
    nrf_twim_disable(twim);
    /* Hand the old pins back. They were left driving open-drain with a
       pull-up, and the application may want them for something else. */
    nrf_gpio_cfg_default((uint32_t)units[unit_num].sda_pin);
    nrf_gpio_cfg_default((uint32_t)units[unit_num].scl_pin);
  }

  /*
   * Both lines are open-drain with the input buffer connected: the bus is
   * wired-AND, so a push-pull driver would fight the pull-ups and any
   * other master. S0D1 drives low and disconnects high. Pull-ups are
   * enabled as a courtesy for boards without external ones -- they are
   * far too weak (~13 kOhm) for 400 kHz, so a real bus still needs its
   * own, but they keep an unpopulated bus from floating.
   */
  const uint8_t pins[2] = { (uint8_t)sda_pin, (uint8_t)scl_pin };
  for (int i = 0; i < 2; i++) {
    nrf_gpio_cfg(pins[i],
                 NRF_GPIO_PIN_DIR_INPUT,
                 NRF_GPIO_PIN_INPUT_CONNECT,
                 NRF_GPIO_PIN_PULLUP,
                 NRF_GPIO_PIN_S0D1,
                 NRF_GPIO_PIN_NOSENSE);
  }

  nrf_twim_pins_set(twim, (uint32_t)scl_pin, (uint32_t)sda_pin);
  nrf_twim_frequency_set(twim, frequency_from_hz(frequency));
  nrf_twim_enable(twim);

  units[unit_num].sda_pin = sda_pin;
  units[unit_num].scl_pin = scl_pin;
  units[unit_num].initialized = true;
  return I2C_ERROR_NONE;
}

/*
 * Bound the spin two ways. TWIM has no timeout of its own, and a device
 * that never releases SDA would otherwise wedge the VM forever. The
 * cycle budget is approximate on purpose -- loop cost varies with
 * optimisation -- but a timeout only has to fire eventually.
 */
static uint32_t
spin_budget(uint32_t timeout_us)
{
  uint32_t cycles_per_us = SystemCoreClock / 1000000u;
  /*
   * Each iteration is two volatile reads of APB peripheral registers
   * through the AHB bridge, plus the compares and the loop -- call it 16
   * cycles, not the 8 a naive count of instructions suggests. Erring
   * high here would make every failed transfer block the VM for twice
   * the timeout the caller asked for.
   */
  if (timeout_us > (UINT32_MAX / cycles_per_us)) {
    return UINT32_MAX;
  }
  uint32_t budget = (timeout_us * cycles_per_us) / 16u;
  return (budget < 1000u) ? 1000u : budget;
}

/*
 * Bring the transaction to a halt and wait for the bus to be released.
 *
 * RESUME before STOP is not optional: TASKS_STOP is documented as
 * "Must be issued while the TWI master is not suspended", so a bare STOP
 * on a unit that ran into a NACK under a LASTTX_SUSPEND shortcut is
 * discarded and the peripheral sits holding SCL low forever. nrfx pairs
 * the two for the same reason.
 *
 * Then wait for STOPPED. Returning while the STOP condition is still
 * being generated leaves the event to arrive during the *next* transfer,
 * where it reads as an immediate completion of a transfer that moved no
 * bytes at all.
 */
static void
abort_transfer(NRF_TWIM_Type *twim)
{
  nrf_twim_event_clear(twim, NRF_TWIM_EVENT_STOPPED);
  nrf_twim_task_trigger(twim, NRF_TWIM_TASK_RESUME);
  nrf_twim_task_trigger(twim, NRF_TWIM_TASK_STOP);

  /* Generating a STOP takes a bit period or two. Bounded so a bus held
     low by a stuck slave cannot trap us here as well. */
  uint32_t budget = spin_budget(STOP_TIMEOUT_US);
  while (budget-- > 0) {
    if (nrf_twim_event_check(twim, NRF_TWIM_EVENT_STOPPED)) {
      break;
    }
  }
  nrf_twim_event_clear(twim, NRF_TWIM_EVENT_STOPPED);
  /* A NACK on the last byte sets SUSPENDED as well as ERROR; leaving it
     set would make the next transfer's wait finish immediately. */
  nrf_twim_event_clear(twim, NRF_TWIM_EVENT_SUSPENDED);
  (void)nrf_twim_errorsrc_get_and_clear(twim);
}

/*
 * Wait for the transfer to end. Returns 0 on a clean finish, or a
 * negative i2c_status_t. `done_event` is STOPPED for a transfer that
 * releases the bus and SUSPENDED for one that holds it for a repeated
 * start.
 */
static int
wait_for(NRF_TWIM_Type *twim, nrf_twim_event_t done_event, uint32_t timeout_us)
{
  uint32_t budget = spin_budget(timeout_us);

  while (budget-- > 0) {
    if (nrf_twim_event_check(twim, NRF_TWIM_EVENT_ERROR)) {
      nrf_twim_event_clear(twim, NRF_TWIM_EVENT_ERROR);
      abort_transfer(twim);
      return I2C_ERROR_UNDETERMINED;
    }
    if (nrf_twim_event_check(twim, done_event)) {
      nrf_twim_event_clear(twim, done_event);
      return 0;
    }
  }

  abort_transfer(twim);
  return I2C_ERROR_UNDETERMINED;
}

static void
begin_transfer(int unit_num, uint8_t addr)
{
  NRF_TWIM_Type *twim = instances[unit_num];

  nrf_twim_address_set(twim, addr);
  nrf_twim_event_clear(twim, NRF_TWIM_EVENT_STOPPED);
  nrf_twim_event_clear(twim, NRF_TWIM_EVENT_SUSPENDED);
  nrf_twim_event_clear(twim, NRF_TWIM_EVENT_ERROR);
}

/*
 * Arm the transfer, then release the bus.
 *
 * RESUME is triggered unconditionally, as nrfx does on every transfer:
 * on a unit that is not suspended it does nothing, and on one that is,
 * it is the only thing that gets the bus moving again. Tracking the
 * suspend state in software instead would mean any desync -- an aborted
 * transfer, a timeout -- leaves the flag lying and the unit unrecoverable.
 *
 * Order matters: START first so the next transfer is queued, RESUME
 * second so the suspended transaction continues into it rather than
 * ending.
 */
static void
start_and_resume(NRF_TWIM_Type *twim, nrf_twim_task_t start_task)
{
  nrf_twim_task_trigger(twim, start_task);
  nrf_twim_task_trigger(twim, NRF_TWIM_TASK_RESUME);
}

int
I2C_write_timeout_us(int unit_num, uint8_t addr, uint8_t *src, size_t len,
                     bool nostop, uint32_t timeout_us)
{
  if (unit_num < 0 || I2C_UNIT_COUNT <= unit_num || !units[unit_num].initialized) {
    return I2C_ERROR_INVALID_UNIT;
  }
  /*
   * EasyDMA reads from Data RAM only. A buffer in flash -- a frozen
   * literal, say -- would transfer garbage or fault, so refuse it the
   * way Nordic's own driver does rather than sending something wrong.
   */
  if (addr >= 0x80) {
    return I2C_ERROR_UNDETERMINED;  /* 7-bit addressing only */
  }
  /* MAXCNT is 16 bits; a longer request would truncate into the field and
     silently move the wrong amount. */
  if (len > 0xFFFFu) {
    return I2C_ERROR_UNDETERMINED;
  }
  /* Nothing to move. TWIM would never raise LASTTX/LASTRX for an empty
     buffer, so the STOP shortcut would never fire and the wait would run
     the whole timeout before failing. */
  if (len == 0) {
    return 0;
  }
  if (!IS_IN_RAM(src)) {
    return I2C_ERROR_UNDETERMINED;
  }

  NRF_TWIM_Type *twim = instances[unit_num];

  begin_transfer(unit_num, addr);
  nrf_twim_tx_buffer_set(twim, src, len);
  nrf_twim_shorts_set(twim, nostop ? (uint32_t)NRF_TWIM_SHORT_LASTTX_SUSPEND_MASK
                                   : (uint32_t)NRF_TWIM_SHORT_LASTTX_STOP_MASK);
  start_and_resume(twim, NRF_TWIM_TASK_STARTTX);

  int result = wait_for(twim,
                        nostop ? NRF_TWIM_EVENT_SUSPENDED : NRF_TWIM_EVENT_STOPPED,
                        timeout_us);
  if (result < 0) {
    return result;
  }

  /*
   * Report what actually moved, not what was asked for. A premature STOP
   * from the slave ends the transfer early without raising ERROR, and the
   * Ruby layer uses this count as a string length -- returning `len` there
   * would hand back uninitialised buffer as data.
   */
  size_t moved = nrf_twim_txd_amount_get(twim);
  if (moved != len) {
    /* The state machine is left mid-transaction after a short transfer;
       nrfx reinitialises it the same way. */
    nrf_twim_disable(twim);
    nrf_twim_enable(twim);
  }
  return (int)moved;
}

int
I2C_read_timeout_us(int unit_num, uint8_t addr, uint8_t *dst, size_t len,
                    bool nostop, uint32_t timeout_us)
{
  if (unit_num < 0 || I2C_UNIT_COUNT <= unit_num || !units[unit_num].initialized) {
    return I2C_ERROR_INVALID_UNIT;
  }
  if (addr >= 0x80) {
    return I2C_ERROR_UNDETERMINED;  /* 7-bit addressing only */
  }
  /* MAXCNT is 16 bits; a longer request would truncate into the field and
     silently move the wrong amount. */
  if (len > 0xFFFFu) {
    return I2C_ERROR_UNDETERMINED;
  }
  /* Nothing to move. TWIM would never raise LASTTX/LASTRX for an empty
     buffer, so the STOP shortcut would never fire and the wait would run
     the whole timeout before failing. */
  if (len == 0) {
    return 0;
  }
  if (!IS_IN_RAM(dst)) {
    return I2C_ERROR_UNDETERMINED;
  }

  NRF_TWIM_Type *twim = instances[unit_num];

  begin_transfer(unit_num, addr);
  nrf_twim_rx_buffer_set(twim, dst, len);
  /* The HAL enum lists LASTTX_SUSPEND but not LASTRX_SUSPEND, though the
     register has both. nrf_twim_shorts_set takes a raw mask, so use the
     bitfield definition directly rather than losing repeated-start on
     reads to a gap in the SDK's enum. */
  nrf_twim_shorts_set(twim, nostop ? TWIM_SHORTS_LASTRX_SUSPEND_Msk
                                   : (uint32_t)NRF_TWIM_SHORT_LASTRX_STOP_MASK);
  start_and_resume(twim, NRF_TWIM_TASK_STARTRX);

  int result = wait_for(twim,
                        nostop ? NRF_TWIM_EVENT_SUSPENDED : NRF_TWIM_EVENT_STOPPED,
                        timeout_us);
  if (result < 0) {
    return result;
  }

  /*
   * Report what actually moved, not what was asked for. A premature STOP
   * from the slave ends the transfer early without raising ERROR, and the
   * Ruby layer uses this count as a string length -- returning `len` there
   * would hand back uninitialised buffer as data.
   */
  size_t moved = nrf_twim_rxd_amount_get(twim);
  if (moved != len) {
    /* The state machine is left mid-transaction after a short transfer;
       nrfx reinitialises it the same way. */
    nrf_twim_disable(twim);
    nrf_twim_enable(twim);
  }
  return (int)moved;
}
