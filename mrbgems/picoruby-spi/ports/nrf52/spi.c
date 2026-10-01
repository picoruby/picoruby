/*
 * nRF52 port: SPIM0, SPIM1 and SPIM2 in master mode, EasyDMA, blocking.
 *
 * SPIM3 is deliberately not offered. It is the only instance that reaches
 * 32 Mbps and the only one with hardware chip select, but nRF52840
 * anomaly 198 corrupts its transmit data, and Nordic's workaround locks
 * RAM blocks through an undocumented register at 0x40000E00 for the
 * duration of every transfer. That is a driver's job, not a port's, so
 * this port stops at the three instances that need no such thing.
 *
 * Chip select is a plain GPIO driven from mrblib (SPI#select /
 * #deselect), so PSEL.CSN is left disconnected here even where the
 * hardware has it.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_spim.h"

#include "../../include/spi.h"

/*
 * Unit 0 is PICORB_SPI_BITBANG in the shared header, so the hardware
 * instances start at 1 and unit_num - 1 indexes the table.
 */
#define PICORB_SPI_NRF52_SPIM0 1
#define PICORB_SPI_NRF52_SPIM2 3
#define SPI_INSTANCE_COUNT     3

/* EasyDMA reads and writes Data RAM only; MAXCNT is 16 bits wide. */
#define IS_IN_RAM(p)     (((uintptr_t)(p) & 0xE0000000u) == 0x20000000u)
#define SPI_MAX_TRANSFER 0xFFFFu

/*
 * A transfer is entirely master-driven -- no slave can stretch the clock
 * as it can on I2C -- so this bound exists only so a misconfigured or
 * powered-down instance cannot wedge the VM.
 *
 * It has to be derived from the configured bit rate, not fixed. This port
 * spans 125 kbit/s to 8 Mbit/s, a factor of 64, and 65535 bytes at the
 * slowest rate is over four seconds -- a fixed budget sized against the
 * fastest would abort legitimate transfers, and at the library's default
 * of 100 kHz (which maps to 125 kbit/s) that is the common case, not an
 * edge one.
 */
#define SPI_SPIN_HEADROOM 4u

static NRF_SPIM_Type *const instances[SPI_INSTANCE_COUNT] = {
  NRF_SPIM0, NRF_SPIM1, NRF_SPIM2
};

static bool initialized[SPI_INSTANCE_COUNT];
static int8_t configured_pins[SPI_INSTANCE_COUNT][3];  /* sck, copi, cipo */
static uint32_t configured_bps[SPI_INSTANCE_COUNT];

static NRF_SPIM_Type *
instance_of(const spi_unit_info_t *unit_info)
{
  int index = (int)unit_info->unit_num - PICORB_SPI_NRF52_SPIM0;

  if (index < 0 || SPI_INSTANCE_COUNT <= index || !initialized[index]) {
    return NULL;
  }
  return instances[index];
}

int
SPI_unit_name_to_unit_num(const char *unit_name)
{
  if (strcmp(unit_name, "NRF52_SPI0") == 0 ||
      strcmp(unit_name, "NRF52_SPIM0") == 0) {
    return PICORB_SPI_NRF52_SPIM0;
  }
  if (strcmp(unit_name, "NRF52_SPI1") == 0 ||
      strcmp(unit_name, "NRF52_SPIM1") == 0) {
    return PICORB_SPI_NRF52_SPIM0 + 1;
  }
  if (strcmp(unit_name, "NRF52_SPI2") == 0 ||
      strcmp(unit_name, "NRF52_SPIM2") == 0) {
    return PICORB_SPI_NRF52_SPIM2;
  }
  return SPI_ERROR_INVALID_UNIT;
}

/*
 * SPIM has a fixed ladder of bit rates. Round down, so a device rated for
 * 4 MHz is not clocked at 8 MHz because the caller asked for 5 -- except
 * below 125 kbit/s, where there is nothing slower to round to and the
 * request is met from above. The library's default of 100 kHz lands
 * there, and runs at 125 kbit/s.
 */
static nrf_spim_frequency_t
frequency_from_hz(uint32_t hz, uint32_t *actual)
{
  *actual = (hz >= 8000000u) ? 8000000u :
            (hz >= 4000000u) ? 4000000u :
            (hz >= 2000000u) ? 2000000u :
            (hz >= 1000000u) ? 1000000u :
            (hz >=  500000u) ?  500000u :
            (hz >=  250000u) ?  250000u : 125000u;
  if (hz >= 8000000u) return NRF_SPIM_FREQ_8M;
  if (hz >= 4000000u) return NRF_SPIM_FREQ_4M;
  if (hz >= 2000000u) return NRF_SPIM_FREQ_2M;
  if (hz >= 1000000u) return NRF_SPIM_FREQ_1M;
  if (hz >=  500000u) return NRF_SPIM_FREQ_500K;
  if (hz >=  250000u) return NRF_SPIM_FREQ_250K;
  return NRF_SPIM_FREQ_125K;
}

spi_status_t
SPI_gpio_init(spi_unit_info_t *unit_info)
{
  int index = (int)unit_info->unit_num - PICORB_SPI_NRF52_SPIM0;

  if (index < 0 || SPI_INSTANCE_COUNT <= index) {
    return SPI_ERROR_INVALID_UNIT;
  }
  if (3 < unit_info->mode) {
    return SPI_ERROR_INVALID_MODE;
  }
  /*
   * The caller's namespace is the Pico SDK's -- mrblib defines
   * MSB_FIRST = 1, LSB_FIRST = 0 -- and nRF numbers them the other way
   * round (MsbFirst = 0). Validate in the caller's domain and map
   * explicitly; casting one enum to the other silently reverses every
   * byte on the wire.
   */
  if (1 < unit_info->first_bit) {
    return SPI_ERROR_INVALID_FIRST_BIT;
  }
  /* SPIM shifts bytes; there is no word-size register to widen. */
  if (unit_info->data_bits != 0 && unit_info->data_bits != 8) {
    return SPI_ERROR_NOT_IMPLEMENTED;
  }
  /* SCK and COPI are driven by the master and must exist. CIPO may be
     left off for a write-only device such as a display or a shift
     register. */
  if (unit_info->sck_pin < 0 || unit_info->copi_pin < 0) {
    return SPI_ERROR_UNDETERMINED;
  }
  if ((uint32_t)unit_info->sck_pin >= NUMBER_OF_PINS ||
      (uint32_t)unit_info->copi_pin >= NUMBER_OF_PINS ||
      (0 <= unit_info->cipo_pin && (uint32_t)unit_info->cipo_pin >= NUMBER_OF_PINS)) {
    return SPI_ERROR_UNDETERMINED;
  }
  /* Distinct pads: PSEL takes any number, and two signals on one pad is a
     dead bus rather than an error the hardware reports. */
  if (unit_info->sck_pin == unit_info->copi_pin ||
      unit_info->sck_pin == unit_info->cipo_pin ||
      unit_info->copi_pin == unit_info->cipo_pin) {
    return SPI_ERROR_UNDETERMINED;
  }

  NRF_SPIM_Type *spim = instances[index];

  if (initialized[index]) {
    nrf_spim_disable(spim);
    /* Hand the previous pins back rather than leaving them driven. */
    for (int i = 0; i < 3; i++) {
      if (0 <= configured_pins[index][i]) {
        nrf_gpio_cfg_default((uint32_t)configured_pins[index][i]);
      }
    }
  }

  /*
   * SCK idles at the level the mode implies -- driving it the other way
   * would clock a spurious edge into the slave the moment CS is asserted.
   * Modes 2 and 3 are the active-low pair.
   */
  if (unit_info->mode >= 2) {
    nrf_gpio_pin_set((uint32_t)unit_info->sck_pin);
  } else {
    nrf_gpio_pin_clear((uint32_t)unit_info->sck_pin);
  }
  /*
   * SCK must have its input buffer connected. The shift logic feeds off
   * the pad, so nrf_gpio_cfg_output -- which disconnects it -- leaves the
   * peripheral unable to complete a transfer. Nordic's own driver spells
   * this out: "this pin and its input buffer must always be connected for
   * the SPI to work".
   */
  nrf_gpio_cfg((uint32_t)unit_info->sck_pin,
               NRF_GPIO_PIN_DIR_OUTPUT,
               NRF_GPIO_PIN_INPUT_CONNECT,
               NRF_GPIO_PIN_NOPULL,
               NRF_GPIO_PIN_S0S1,
               NRF_GPIO_PIN_NOSENSE);

  nrf_gpio_pin_clear((uint32_t)unit_info->copi_pin);
  nrf_gpio_cfg_output((uint32_t)unit_info->copi_pin);

  if (0 <= unit_info->cipo_pin) {
    /* Pulled down, as nrfx defaults to on this part: a tri-stated slave
       or a deselected bus would otherwise float and read as noise. */
    nrf_gpio_cfg_input((uint32_t)unit_info->cipo_pin, NRF_GPIO_PIN_PULLDOWN);
  }

  nrf_spim_pins_set(spim,
                    (uint32_t)unit_info->sck_pin,
                    (uint32_t)unit_info->copi_pin,
                    (0 <= unit_info->cipo_pin) ? (uint32_t)unit_info->cipo_pin
                                               : NRF_SPIM_PIN_NOT_CONNECTED);
  nrf_spim_frequency_set(spim, frequency_from_hz(unit_info->frequency,
                                                 &configured_bps[index]));
  nrf_spim_configure(spim,
                     (nrf_spim_mode_t)unit_info->mode,
                     (unit_info->first_bit == 1) ? NRF_SPIM_BIT_ORDER_MSB_FIRST
                                                 : NRF_SPIM_BIT_ORDER_LSB_FIRST);
  nrf_spim_enable(spim);

  configured_pins[index][0] = unit_info->sck_pin;
  configured_pins[index][1] = unit_info->copi_pin;
  configured_pins[index][2] = unit_info->cipo_pin;
  initialized[index] = true;
  return SPI_ERROR_NONE;
}

/*
 * Run one EasyDMA transfer and wait for it.
 *
 * SPIM clocks max(TXD.MAXCNT, RXD.MAXCNT) bytes: the shorter direction is
 * padded, transmitting ORC or discarding the surplus received bytes. That
 * is what makes a read a transfer with no TX buffer, and a write a
 * transfer with no RX buffer.
 */
static int
transfer(NRF_SPIM_Type *spim, uint32_t bits_per_second,
         const uint8_t *tx, size_t tx_len,
         uint8_t *rx, size_t rx_len)
{
  nrf_spim_tx_buffer_set(spim, tx, tx_len);
  nrf_spim_rx_buffer_set(spim, rx, rx_len);
  nrf_spim_event_clear(spim, NRF_SPIM_EVENT_END);
  nrf_spim_task_trigger(spim, NRF_SPIM_TASK_START);

  /* Cycles the transfer should take, times a generous headroom. The
     spin body costs several cycles per iteration, so counting iterations
     against a cycle budget errs long -- which is the safe direction. */
  size_t clocked = (tx_len > rx_len) ? tx_len : rx_len;
  uint64_t budget = ((uint64_t)clocked * 8u * SystemCoreClock * SPI_SPIN_HEADROOM)
                    / (bits_per_second ? bits_per_second : 125000u);
  if (budget > UINT32_MAX) {
    budget = UINT32_MAX;
  }

  while (budget-- > 0) {
    if (nrf_spim_event_check(spim, NRF_SPIM_EVENT_END)) {
      nrf_spim_event_clear(spim, NRF_SPIM_EVENT_END);
      return 0;
    }
  }

  /*
   * Stop and wait for it. Returning while EasyDMA is still live would
   * leave it writing into the caller's buffer -- a stack VLA in both glue
   * layers -- after the frame has been unwound by the raised exception,
   * and would leave a stale END to make the next transfer report an
   * instant success that moved nothing.
   */
  nrf_spim_event_clear(spim, NRF_SPIM_EVENT_STOPPED);
  nrf_spim_task_trigger(spim, NRF_SPIM_TASK_STOP);
  for (uint32_t guard = 1000000u; guard > 0; guard--) {
    if (nrf_spim_event_check(spim, NRF_SPIM_EVENT_STOPPED)) {
      break;
    }
  }
  nrf_spim_event_clear(spim, NRF_SPIM_EVENT_STOPPED);
  nrf_spim_event_clear(spim, NRF_SPIM_EVENT_END);
  return SPI_ERROR_UNDETERMINED;
}

static int
check_buffer(const void *buf, size_t len)
{
  if (len == 0) {
    return 0;
  }
  if (SPI_MAX_TRANSFER < len) {
    return SPI_ERROR_UNDETERMINED;
  }
  if (!IS_IN_RAM(buf)) {
    return SPI_ERROR_UNDETERMINED;
  }
  return 0;
}

int
SPI_read_blocking(spi_unit_info_t *unit_info, uint8_t *dst, size_t len,
                  uint8_t repeated_tx_data)
{
  NRF_SPIM_Type *spim = instance_of(unit_info);
  if (spim == NULL) {
    return SPI_ERROR_INVALID_UNIT;
  }
  int status = check_buffer(dst, len);
  if (status < 0) {
    return status;
  }
  if (len == 0) {
    return 0;
  }

  /*
   * ORC is exactly this call's contract: the byte SPIM shifts out while
   * the receive buffer is longer than the transmit one. With no TX buffer
   * at all, every clocked byte is ORC.
   */
  nrf_spim_orc_set(spim, repeated_tx_data);

  status = transfer(spim, configured_bps[unit_info->unit_num - PICORB_SPI_NRF52_SPIM0], NULL, 0, dst, len);
  return (status < 0) ? status : (int)len;
}

int
SPI_write_blocking(spi_unit_info_t *unit_info, uint8_t *src, size_t len)
{
  NRF_SPIM_Type *spim = instance_of(unit_info);
  if (spim == NULL) {
    return SPI_ERROR_INVALID_UNIT;
  }
  int status = check_buffer(src, len);
  if (status < 0) {
    return status;
  }
  if (len == 0) {
    return 0;
  }

  status = transfer(spim, configured_bps[unit_info->unit_num - PICORB_SPI_NRF52_SPIM0], src, len, NULL, 0);
  return (status < 0) ? status : (int)len;
}

int
SPI_transfer(spi_unit_info_t *unit_info, uint8_t *src, uint8_t *dst, size_t len)
{
  NRF_SPIM_Type *spim = instance_of(unit_info);
  if (spim == NULL) {
    return SPI_ERROR_INVALID_UNIT;
  }
  int status = check_buffer(src, len);
  if (status < 0) {
    return status;
  }
  status = check_buffer(dst, len);
  if (status < 0) {
    return status;
  }
  if (len == 0) {
    return 0;
  }

  status = transfer(spim, configured_bps[unit_info->unit_num - PICORB_SPI_NRF52_SPIM0], src, len, dst, len);
  return (status < 0) ? status : (int)len;
}
