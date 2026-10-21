/*
 * nRF52 port: UARTE0 and UARTE1.
 *
 * UARTE is EasyDMA-driven, so it has no readable FIFO the way the RP2040
 * UART does. The receiver runs as a one-byte DMA that re-arms itself
 * through the ENDRX_STARTRX shortcut: every character raises ENDRX, the
 * handler copies it into the gem's ring, and the hardware is already
 * receiving the next one. That is the shape this contract wants -- the
 * shared layer owns the ring, the port only has to produce bytes.
 *
 * The cost is one interrupt per character, and a byte is lost if the
 * handler cannot read the landing buffer within one character time
 * (87 us at 115200, 8.7 us at 1 Mbaud). Fine for a console or a GPS
 * module, not for sustained high-rate bursts; a port that needs those
 * would have to double-buffer and count RXDRDY.
 *
 * Note this is a *peripheral* UART, unrelated to the console. On R2P2-nRF
 * the console is USB CDC and lives in the product's own HAL, so both
 * UARTE instances are free here.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_uarte.h"

#include "../../include/uart.h"

#define PICORB_UART_NRF52_UARTE0 0
#define PICORB_UART_NRF52_UARTE1 1
#define UART_INSTANCE_COUNT      2

#define UART_TX_CHUNK_MAX 64u

static NRF_UARTE_Type *const instances[UART_INSTANCE_COUNT] = {
  NRF_UARTE0, NRF_UARTE1
};

static const IRQn_Type irq_numbers[UART_INSTANCE_COUNT] = {
  UARTE0_UART0_IRQn, UARTE1_IRQn
};

/* One landing byte per unit. EasyDMA writes here, so it must be RAM and
   must outlive the transfer. */
static volatile uint8_t rx_byte[UART_INSTANCE_COUNT];

/*
 * A transmit buffer that is guaranteed to be in RAM. Unlike picoruby-i2c,
 * the UART glue hands RSTRING_PTR() straight through, so a string that
 * lives in flash would reach EasyDMA and transmit nothing useful. Copying
 * in chunks costs a memcpy per 64 bytes against 5.5 ms of line time at
 * 115200 -- not worth detecting the RAM case to avoid.
 */
static uint8_t tx_chunk[UART_INSTANCE_COUNT][UART_TX_CHUNK_MAX];

static bool opened[UART_INSTANCE_COUNT];
static bool producer_started[UART_INSTANCE_COUNT];
static uint32_t current_baudrate[UART_INSTANCE_COUNT];

static void
drain_rx(int unit_num)
{
  NRF_UARTE_Type *uarte = instances[unit_num];
  RingBuffer *rx = UART_unit_rx(unit_num);
  bool stored = false;

  while (nrf_uarte_event_check(uarte, NRF_UARTE_EVENT_ENDRX)) {
    /*
     * Read the byte first, then clear. Clearing first opens a window in
     * which the shortcut's restart can land the next character and raise
     * ENDRX again -- the loop would then read the same byte twice and
     * invent a character that was never sent. Read-then-clear can only
     * ever drop one, which is the honest failure for a receiver that
     * fell a whole character time behind.
     */
    uint8_t ch = rx_byte[unit_num];
    bool complete = (nrf_uarte_rx_amount_get(uarte) == 1);
    nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_ENDRX);

    /* A transfer cut short by STOPRX reports fewer bytes than the buffer
       holds; whatever is in the landing byte then is stale. */
    if (complete && rx && UART_pushBuffer(rx, ch)) {
      stored = true;
    }
  }
  /* Signal once for the burst, and only when something is actually
     there to read: a dropped byte is nothing new to drain. */
  if (stored) {
    UART_signal_rx(unit_num);
  }
}

void
UARTE0_UART0_IRQHandler(void)
{
  drain_rx(PICORB_UART_NRF52_UARTE0);
}

void
UARTE1_IRQHandler(void)
{
  drain_rx(PICORB_UART_NRF52_UARTE1);
}

int
UART_unit_name_to_unit_num(const char *name)
{
  if (strcmp(name, "NRF52_UART0") == 0 || strcmp(name, "NRF52_UARTE0") == 0) {
    return PICORB_UART_NRF52_UARTE0;
  }
  if (strcmp(name, "NRF52_UART1") == 0 || strcmp(name, "NRF52_UARTE1") == 0) {
    return PICORB_UART_NRF52_UARTE1;
  }
  return UART_ERROR_INVALID_UNIT;
}

/*
 * The baud rate register holds a pre-computed divisor, not a frequency,
 * so only the tabulated rates exist. Pick the nearest rather than
 * rounding one way: a UART link fails at roughly 2% error in either
 * direction, so being deliberately low is no safer than being high.
 */
static nrf_uarte_baudrate_t
baudrate_from_hz(uint32_t baudrate, uint32_t *actual)
{
  static const struct { uint32_t hz; nrf_uarte_baudrate_t reg; } table[] = {
    {   1200, NRF_UARTE_BAUDRATE_1200   },
    {   2400, NRF_UARTE_BAUDRATE_2400   },
    {   4800, NRF_UARTE_BAUDRATE_4800   },
    {   9600, NRF_UARTE_BAUDRATE_9600   },
    {  14400, NRF_UARTE_BAUDRATE_14400  },
    {  19200, NRF_UARTE_BAUDRATE_19200  },
    {  28800, NRF_UARTE_BAUDRATE_28800  },
    {  31250, NRF_UARTE_BAUDRATE_31250  },
    {  38400, NRF_UARTE_BAUDRATE_38400  },
    {  56000, NRF_UARTE_BAUDRATE_56000  },
    {  57600, NRF_UARTE_BAUDRATE_57600  },
    {  76800, NRF_UARTE_BAUDRATE_76800  },
    { 115200, NRF_UARTE_BAUDRATE_115200 },
    { 230400, NRF_UARTE_BAUDRATE_230400 },
    { 250000, NRF_UARTE_BAUDRATE_250000 },
    { 460800, NRF_UARTE_BAUDRATE_460800 },
    { 921600, NRF_UARTE_BAUDRATE_921600 },
    {1000000, NRF_UARTE_BAUDRATE_1000000},
  };
  size_t best = 0;
  uint32_t best_error = UINT32_MAX;

  for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
    uint32_t error = (table[i].hz > baudrate) ? (table[i].hz - baudrate)
                                              : (baudrate - table[i].hz);
    if (error < best_error) {
      best_error = error;
      best = i;
    }
  }
  *actual = table[best].hz;
  return table[best].reg;
}

uint32_t
UART_set_baudrate(int unit_num, uint32_t baudrate)
{
  if (unit_num < 0 || UART_INSTANCE_COUNT <= unit_num) {
    return 0;
  }
  uint32_t actual;
  nrf_uarte_baudrate_set(instances[unit_num], baudrate_from_hz(baudrate, &actual));
  current_baudrate[unit_num] = actual;
  return actual;
}

void
UART_open(int unit_num, uint32_t txd_pin, uint32_t rxd_pin)
{
  if (unit_num < 0 || UART_INSTANCE_COUNT <= unit_num) {
    return;
  }
  if (txd_pin >= NUMBER_OF_PINS && rxd_pin >= NUMBER_OF_PINS) {
    return;  /* a unit with neither direction has nothing to open */
  }

  NRF_UARTE_Type *uarte = instances[unit_num];

  /*
   * Reopening applies new pins and format but must not start a second
   * producer -- hence the split with producer_started below.
   *
   * Stopping the receiver takes more than STOPRX: the ENDRX_STARTRX
   * shortcut would turn the ENDRX that STOPRX raises straight back into
   * a restart, so the stop would not stick. Nordic's driver disables the
   * short first for exactly this reason, then waits for RXTO before
   * touching ENABLE.
   */
  if (opened[unit_num]) {
    if (producer_started[unit_num]) {
      NVIC_DisableIRQ(irq_numbers[unit_num]);
      nrf_uarte_shorts_disable(uarte, NRF_UARTE_SHORT_ENDRX_STARTRX);
      nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_RXTO);
      nrf_uarte_task_trigger(uarte, NRF_UARTE_TASK_STOPRX);
      for (uint32_t guard = 1000000u; guard > 0; guard--) {
        if (nrf_uarte_event_check(uarte, NRF_UARTE_EVENT_RXTO)) {
          break;
        }
      }
      nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_RXTO);
      /* The stopped transfer leaves ENDRX set over a stale landing byte.
         Drop both rather than let the handler deliver it again. */
      nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_ENDRX);
    }
    nrf_uarte_disable(uarte);
  }

  /* TXD idles high; driving it low would look like a start bit to the
     device at the other end the moment the pin is configured. */
  if (txd_pin < NUMBER_OF_PINS) {
    nrf_gpio_pin_set(txd_pin);
    nrf_gpio_cfg_output(txd_pin);
  }
  if (rxd_pin < NUMBER_OF_PINS) {
    nrf_gpio_cfg_input(rxd_pin, NRF_GPIO_PIN_NOPULL);
  }
  nrf_uarte_txrx_pins_set(uarte,
    (txd_pin < NUMBER_OF_PINS) ? txd_pin : NRF_UARTE_PSEL_DISCONNECTED,
    (rxd_pin < NUMBER_OF_PINS) ? rxd_pin : NRF_UARTE_PSEL_DISCONNECTED);

  /*
   * Baud rate and frame format go in before ENABLE. nrf_uarte_configure
   * overwrites CONFIG wholesale, so it has to run before any format the
   * caller set, and enabling the receiver before both are in place would
   * put whatever is on the wire into the ring at the wrong settings.
   */
  if (current_baudrate[unit_num] == 0) {
    UART_set_baudrate(unit_num, DEFAULT_BAUDRATE);
  } else {
    UART_set_baudrate(unit_num, current_baudrate[unit_num]);
  }
  if (!opened[unit_num]) {
    nrf_uarte_configure(uarte, NRF_UARTE_PARITY_EXCLUDED, NRF_UARTE_HWFC_DISABLED);
  }
  nrf_uarte_enable(uarte);

  opened[unit_num] = true;

  if (rxd_pin < NUMBER_OF_PINS) {
    /*
     * One byte per DMA, re-armed in hardware. Setting the shortcut before
     * the first STARTRX means there is never a gap where a character
     * could arrive with no buffer pointed at it.
     */
    nrf_uarte_rx_buffer_set(uarte, (uint8_t *)&rx_byte[unit_num], 1);
    nrf_uarte_shorts_enable(uarte, NRF_UARTE_SHORT_ENDRX_STARTRX);
    nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_ENDRX);
    nrf_uarte_int_enable(uarte, NRF_UARTE_INT_ENDRX_MASK);
    /* Priority 6, matching app_timer and nrfx_usbd. The default of 0
       would let a character interrupt preempt the USB stack. */
    NVIC_SetPriority(irq_numbers[unit_num], 6);
    NVIC_ClearPendingIRQ(irq_numbers[unit_num]);
    NVIC_EnableIRQ(irq_numbers[unit_num]);
    nrf_uarte_task_trigger(uarte, NRF_UARTE_TASK_STARTRX);
    producer_started[unit_num] = true;
  } else {
    /* Reopened with no RXD: leave the receiver stopped rather than
       restarting it against a disconnected pin. */
    producer_started[unit_num] = false;
  }
}

void
UART_set_flow_control(int unit_num, bool cts, bool rts)
{
  /* Not supported. UARTE has hardware flow control, but this contract
   * carries no CTS/RTS pin numbers and nRF52 has no fixed pins to fall
   * back on -- every signal is routed by writing a pin into PSEL. There
   * is nothing to enable. */
  (void)unit_num; (void)cts; (void)rts;
}

int
UART_set_format(int unit_num, uint32_t data_bits, uint32_t stop_bits, uint8_t parity)
{
  if (unit_num < 0 || UART_INSTANCE_COUNT <= unit_num) {
    return UART_ERROR_INVALID_UNIT;
  }
  /*
   * UARTE frames 8 data bits and nothing else, and its parity is
   * even-or-none: PARITYTYPE does not exist on this part. Say so rather
   * than leave the format as it was and answer success -- a caller that
   * asks for 8O2 and keeps running at 8N1 sees framing errors on the
   * wire with nothing anywhere to explain them, and this port does not
   * report wire errors either.
   */
  if (data_bits != 8 || parity == PARITY_ODD) {
    return UART_ERROR_INVALID_FORMAT;
  }

  NRF_UARTE_Type *uarte = instances[unit_num];
  uint32_t config = uarte->CONFIG;

  config &= ~(uint32_t)(UARTE_CONFIG_PARITY_Msk | UARTE_CONFIG_STOP_Msk);
  if (parity == PARITY_EVEN) {
    config |= (uint32_t)NRF_UARTE_PARITY_INCLUDED;
  }
  /* The HAL's configure() only reaches parity and flow control, so the
     stop-bit field is written directly. */
  config |= ((stop_bits == 2) ? UARTE_CONFIG_STOP_Two : UARTE_CONFIG_STOP_One)
            << UARTE_CONFIG_STOP_Pos;
  uarte->CONFIG = config;
  return UART_ERROR_NONE;
}

void
UART_set_function(uint32_t pin)
{
  /* Not supported, and nothing to support: nRF52 has no per-pin function
   * mux. The peripheral claims the pin through PSEL, which UART_open
   * already did. */
  (void)pin;
}

bool
UART_is_writable(int unit_num)
{
  /* Writes block until EasyDMA reports the buffer sent, so a unit that is
     open can always accept more. */
  return (0 <= unit_num && unit_num < UART_INSTANCE_COUNT && opened[unit_num]);
}

void
UART_write_blocking(int unit_num, const uint8_t *src, size_t len)
{
  if (unit_num < 0 || UART_INSTANCE_COUNT <= unit_num || !opened[unit_num]) {
    return;
  }

  NRF_UARTE_Type *uarte = instances[unit_num];
  size_t sent = 0;

  while (sent < len) {
    size_t chunk = len - sent;
    if (UART_TX_CHUNK_MAX < chunk) {
      chunk = UART_TX_CHUNK_MAX;
    }
    memcpy(tx_chunk[unit_num], &src[sent], chunk);

    nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_ENDTX);
    nrf_uarte_tx_buffer_set(uarte, tx_chunk[unit_num], (uint8_t)chunk);
    nrf_uarte_task_trigger(uarte, NRF_UARTE_TASK_STARTTX);

    /*
     * Bounded so a unit whose TXD was never routed cannot wedge the VM.
     * 64 bytes at the slowest rate this port offers (1200 baud) is about
     * 530 ms; the budget is generous against that and irrelevant at any
     * realistic rate.
     */
    uint32_t budget = 40000000u;
    while (!nrf_uarte_event_check(uarte, NRF_UARTE_EVENT_ENDTX)) {
      if (budget-- == 0) {
        /* Wait for the DMA to actually stop before returning: the next
           call rewrites TXD.PTR, and EasyDMA must not still be reading
           the chunk buffer when it does. */
        nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_TXSTOPPED);
        nrf_uarte_task_trigger(uarte, NRF_UARTE_TASK_STOPTX);
        for (uint32_t g = 1000000u; g > 0; g--) {
          if (nrf_uarte_event_check(uarte, NRF_UARTE_EVENT_TXSTOPPED)) {
            break;
          }
        }
        nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_TXSTOPPED);
        return;
      }
    }
    nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_ENDTX);
    /*
     * Stop the transmitter between chunks. UARTE keeps drawing current
     * while TX is running -- Nordic triggers STOPTX on every ENDTX for
     * that reason -- and leaving it live means a later disable tears down
     * a running state machine.
     */
    nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_TXSTOPPED);
    nrf_uarte_task_trigger(uarte, NRF_UARTE_TASK_STOPTX);
    for (uint32_t guard = 1000000u; guard > 0; guard--) {
      if (nrf_uarte_event_check(uarte, NRF_UARTE_EVENT_TXSTOPPED)) {
        break;
      }
    }
    nrf_uarte_event_clear(uarte, NRF_UARTE_EVENT_TXSTOPPED);
    sent += chunk;
  }
}

bool
UART_is_readable(int unit_num)
{
  /* There is no readable FIFO to consult: EasyDMA has already moved every
     received byte into the gem's ring, which the shared layer owns. */
  (void)unit_num;
  return false;
}

size_t
UART_read_nonblocking(int unit_num, uint8_t *dst, size_t len)
{
  /* Same reason as UART_is_readable: reads come from the ring, not from
     the peripheral. */
  (void)unit_num; (void)dst; (void)len;
  return 0;
}

void
UART_break(int unit_num, uint32_t interval)
{
  /* Not supported: UARTE can only send a break by holding TXD low, and
   * it offers no task to do so. Emulating it would mean unrouting the
   * pin, driving it, and rerouting -- which races the receiver. */
  (void)unit_num; (void)interval;
}

void
UART_flush(int unit_num)
{
  /* Writes are already blocking, so anything handed to this port has left
     the shift register by the time the call returned. */
  (void)unit_num;
}

void
UART_clear_rx_buffer(int unit_num)
{
  if (unit_num < 0 || UART_INSTANCE_COUNT <= unit_num) {
    return;
  }
  /* The ring itself is the shared layer's to clear. All that belongs here
     is the byte the DMA may have landed but the handler has not taken. */
  nrf_uarte_event_clear(instances[unit_num], NRF_UARTE_EVENT_ENDRX);
}

void
UART_clear_tx_buffer(int unit_num)
{
  /* Nothing to clear: there is no queue behind a blocking write. */
  (void)unit_num;
}
