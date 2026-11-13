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

/*
 * The TWIM HAL rather than nrfx_twim, because the driver cannot meet this
 * gem's contract. I2C_read_timeout_us and I2C_write_timeout_us take the
 * caller's timeout, and nrfx_twim's blocking path is
 *
 *     do { ... } while (!transmission_finished);
 *
 * (nrfx_twim.c) with no bound of any kind, so a device holding SCL low
 * takes the VM with it. Reaching for the driver's non-blocking mode
 * instead means an event handler, a completion flag and this same timeout
 * loop wrapped around it -- more code to end up where the lines below
 * already are.
 */

#define I2C_UNIT_COUNT TWIM_COUNT

/* nRF52840 Data RAM is 0x20000000..0x2003FFFF. */
#define IS_IN_RAM(p) (((uintptr_t)(p) & 0xE0000000u) == 0x20000000u)
/* Long enough for the STOP condition itself, short enough that a stuck
   bus does not double the caller's timeout. */
#define STOP_TIMEOUT_US 1000u

static NRF_TWIM_Type *const instances[I2C_UNIT_COUNT] = { NRF_TWIM0, NRF_TWIM1 };

typedef struct {
  bool     initialized;
  bool     holding;      /* suspended mid-transaction for a repeated start */
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
 *
 * 100 kHz is the floor, and a request below it is the one case met from
 * above -- there is nothing slower to round to. A caller that derated
 * its device below the standard rate therefore does not get what it
 * asked for, which is the one deviation from the rule above.
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
  /* One pad cannot carry both lines. nrfx asserts on this; without the
     assert the unit is programmed with SCL and SDA on the same pin and
     every transfer spins out its whole timeout with nothing on the bus
     to explain why. */
  if (sda_pin == scl_pin) {
    return I2C_ERROR_UNDETERMINED;
  }
  /*
   * TWIM0/1 and SPIM0/1 are one peripheral, not two: NRF_TWIM0_BASE ==
   * NRF_SPIM0_BASE, and SPIM's PSEL.SCK/MOSI sit at the same offsets as
   * TWIM's PSEL.SCL/SDA. Enabling TWIM over a live SPI unit therefore
   * repoints its pins and changes what the registers mean, leaving
   * picoruby-spi driving a peripheral that is no longer a SPIM -- and
   * neither gem's state says anything about why its transfers stopped
   * completing. ENABLE is the one register that reads back which
   * personality is active (TWIM 6, SPIM 7), so ask it and refuse rather
   * than take the peripheral over.
   */
  if (instances[unit_num]->ENABLE == SPIM_ENABLE_ENABLE_Enabled) {
    return I2C_ERROR_UNDETERMINED;
  }

  NRF_TWIM_Type *twim = instances[unit_num];

  if (units[unit_num].initialized) {
    /*
     * Another I2C object already holds this unit. Reconfiguring it for a
     * different pair of pins used to hand the old ones back to the GPIO
     * defaults, which releases the lines of a bus that object is still
     * using while it goes on reporting its original pins. The same pins
     * are a reconfiguration of one bus and allowed; a different pair is
     * refused, since this gem has no close for the first object to have
     * released it with.
     */
    if (units[unit_num].sda_pin != sda_pin || units[unit_num].scl_pin != scl_pin) {
      return I2C_ERROR_UNDETERMINED;
    }
    /* Same pins, reconfigured below. Stop driving first: PSEL and
       FREQUENCY are written while the unit is disabled. */
    nrf_twim_disable(twim);
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
  /* Disabling the unit above released the lines, so whatever a previous
     configuration was holding is gone with it. */
  units[unit_num].holding = false;
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

/*
 * End a transaction that a previous phase left suspended for a repeated
 * start.
 *
 * Every path that gives up before arming a transfer of its own has to do
 * this. The bus is still held, so SCL stays low for as long as the
 * program runs, and the next unrelated transfer would RESUME into the
 * abandoned transaction rather than starting its own -- a write to one
 * address emitted as a repeated start inside another address's
 * transaction.
 *
 * Tracked in software because the SUSPENDED event is cleared by the wait
 * that observed it, so the peripheral offers no way to ask afterwards.
 * Used only to decide whether to release, never to decide whether to
 * RESUME: start_and_resume does that unconditionally, for the reason
 * given above it.
 */
static void
release_if_holding(int unit_num)
{
  if (!units[unit_num].holding) {
    return;
  }
  abort_transfer(instances[unit_num]);
  units[unit_num].holding = false;
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
  /*
   * Each of the refusals below releases the bus first. A preceding phase
   * may have left the transaction suspended for a repeated start, and
   * returning from here without ending it leaves SCL low indefinitely.
   */
  if (addr >= 0x80) {
    release_if_holding(unit_num);
    return I2C_ERROR_UNDETERMINED;  /* 7-bit addressing only */
  }
  /* MAXCNT is 16 bits; a longer request would truncate into the field and
     silently move the wrong amount. */
  if (len > 0xFFFFu) {
    release_if_holding(unit_num);
    return I2C_ERROR_UNDETERMINED;
  }
  /* Nothing to move. TWIM would never raise LASTTX/LASTRX for an empty
     buffer, so the STOP shortcut would never fire and the wait would run
     the whole timeout before failing. Releasing is the right answer even
     for nostop: a phase that moves no bytes cannot hold a transaction
     open for one that never comes. */
  if (len == 0) {
    release_if_holding(unit_num);
    return 0;
  }
  if (!IS_IN_RAM(src)) {
    release_if_holding(unit_num);
    return I2C_ERROR_UNDETERMINED;
  }

  NRF_TWIM_Type *twim = instances[unit_num];

  begin_transfer(unit_num, addr);
  /* The suspended transaction, if there was one, is being continued into
     this transfer; whether the bus stays held is decided below. */
  units[unit_num].holding = false;
  nrf_twim_tx_buffer_set(twim, src, len);
  nrf_twim_shorts_set(twim, nostop ? (uint32_t)NRF_TWIM_SHORT_LASTTX_SUSPEND_MASK
                                   : (uint32_t)NRF_TWIM_SHORT_LASTTX_STOP_MASK);
  start_and_resume(twim, NRF_TWIM_TASK_STARTTX);

  int result = wait_for(twim,
                        nostop ? NRF_TWIM_EVENT_SUSPENDED : NRF_TWIM_EVENT_STOPPED,
                        timeout_us);
  if (result < 0) {
    /* wait_for aborted the transaction, so nothing is held. */
    return result;
  }
  units[unit_num].holding = nostop;

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
    /*
     * That power cycle took the suspended transaction with it, so a
     * caller holding the bus for a repeated start has nothing left to
     * continue into. Report it: returning the short count would let the
     * next phase open a fresh transaction the slave reads as unrelated
     * -- a read with no register address latched, answered from wherever
     * its internal pointer happens to sit.
     */
    units[unit_num].holding = false;
    if (nostop) {
      return I2C_ERROR_UNDETERMINED;
    }
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
  /*
   * Each of the refusals below releases the bus first. A preceding phase
   * may have left the transaction suspended for a repeated start, and
   * returning from here without ending it leaves SCL low indefinitely.
   */
  if (addr >= 0x80) {
    release_if_holding(unit_num);
    return I2C_ERROR_UNDETERMINED;  /* 7-bit addressing only */
  }
  /* MAXCNT is 16 bits; a longer request would truncate into the field and
     silently move the wrong amount. */
  if (len > 0xFFFFu) {
    release_if_holding(unit_num);
    return I2C_ERROR_UNDETERMINED;
  }
  /* Nothing to move. TWIM would never raise LASTTX/LASTRX for an empty
     buffer, so the STOP shortcut would never fire and the wait would run
     the whole timeout before failing. Releasing is the right answer even
     for nostop: a phase that moves no bytes cannot hold a transaction
     open for one that never comes. */
  if (len == 0) {
    release_if_holding(unit_num);
    return 0;
  }
  if (!IS_IN_RAM(dst)) {
    release_if_holding(unit_num);
    return I2C_ERROR_UNDETERMINED;
  }

  NRF_TWIM_Type *twim = instances[unit_num];

  begin_transfer(unit_num, addr);
  /* The suspended transaction, if there was one, is being continued into
     this transfer; whether the bus stays held is decided below. */
  units[unit_num].holding = false;
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
    /* wait_for aborted the transaction, so nothing is held. */
    return result;
  }
  units[unit_num].holding = nostop;

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
    /*
     * That power cycle took the suspended transaction with it, so a
     * caller holding the bus for a repeated start has nothing left to
     * continue into. Report it: returning the short count would let the
     * next phase open a fresh transaction the slave reads as unrelated
     * -- a read with no register address latched, answered from wherever
     * its internal pointer happens to sit.
     */
    units[unit_num].holding = false;
    if (nostop) {
      return I2C_ERROR_UNDETERMINED;
    }
  }
  return (int)moved;
}
