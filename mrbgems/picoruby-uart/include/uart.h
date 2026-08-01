#ifndef UART_DEFINED_H_
#define UART_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PARITY_NONE   0
#define PARITY_EVEN   1
#define PARITY_ODD    2
#define FLOW_CONTROL_NONE       0
#define FLOW_CONTROL_RTS_CTS    1

#define DEFAULT_BAUDRATE 115200

#define PICORB_UART_RP2040_UART0      0
#define PICORB_UART_RP2040_UART1      1

/*
 * Size of the shared unit table. This is an upper bound, not a
 * validation: an ESP32 variant may have fewer units than UART_NUM_MAX
 * allows for here, so the port checks the unit against its own chip
 * limit in UART_open() as well.
 */
#if defined(PICORB_PLATFORM_RP2)
#define UART_UNIT_MAX 2
#elif defined(PICORB_PLATFORM_ESP32)
#define UART_UNIT_MAX 3
#else
#define UART_UNIT_MAX 1
#endif

typedef enum {
 UART_ERROR_NONE          =  0,
 UART_ERROR_INVALID_UNIT  = -1,
 UART_ERROR_UNIT_MISMATCH = -2,
 UART_ERROR_UNDETERMINED  = -3,
 UART_ERROR_BUFFER_SIZE   = -4,
 UART_ERROR_BUFFER_INUSE  = -5,
 UART_ERROR_NOMEM         = -6,
} uart_status_t;

typedef void (*PushBuffer)(RingBuffer *ring_buffer, uint8_t ch);

size_t UART_rx_buffer_allocation_size(size_t capacity);
bool UART_rx_buffer_init(RingBuffer *ring_buffer, size_t capacity);

/*
 * Open the unit's RX ring, allocating it the first time. Reopening the
 * same unit at the same capacity reuses the ring; at a different
 * capacity it fails with UART_ERROR_BUFFER_INUSE rather than freeing a
 * buffer the producer may be writing into.
 */
int UART_unit_open(int unit_num, size_t capacity);

/* The producer's handle on the ring. NULL until the unit is open. */
RingBuffer *UART_unit_rx(int unit_num);

/* Producer side: the port's ISR or RX task. */
bool UART_pushBuffer(RingBuffer *ring_buffer, uint8_t ch);
bool UART_pushBufferAt(RingBuffer *ring_buffer, uint8_t ch, uint32_t timestamp_us);

/* Consumer side: VM context only. */
size_t UART_bytes_available(int unit_num);
bool UART_getbyte(int unit_num, uint8_t *ch);
size_t UART_read(int unit_num, uint8_t *dst, size_t len);
int UART_line_length(int unit_num);
bool UART_ungetbyte(int unit_num, uint8_t ch);
void UART_clear_rx(int unit_num);
bool UART_lastReadTimestamp(int unit_num, uint64_t *timestamp_us);
uint32_t UART_rxOverflowCount(int unit_num);

int UART_unit_name_to_unit_num(const char *unit_name);
/*
 * Resolve the unit number from the given unit name and pins.
 * On RP2 the unit can be inferred from the pins when unit_name is empty, and a
 * mismatch between unit_name and pins (or between pins) yields a negative error
 * code. On other platforms this falls back to UART_unit_name_to_unit_num().
 */
int UART_resolve_unit_num(const char *unit_name, int txd_pin, int rxd_pin);
int UART_unit_num_from_pins(const char *unit_name, int txd_pin, int rxd_pin);

/*
 * Configure the unit, and start its producer the first time. Splitting
 * those apart matters: reopening a unit must still apply the new pins
 * and format, but must not install a second IRQ handler or RX task.
 */
void UART_open(int unit_num, uint32_t txd_pin, uint32_t rxd_pin);
uint32_t UART_set_baudrate(int unit_num, uint32_t baudrate);
void UART_set_flow_control(int unit_num, bool cts, bool rts);
void UART_set_format(int unit_num, uint32_t data_bits, uint32_t stop_bits, uint8_t parity);
void UART_set_function(uint32_t pin);
void UART_write_blocking(int unit_num, const uint8_t *src, size_t len);
bool UART_is_readable(int unit_num);
size_t UART_read_nonblocking(int unit_num, uint8_t *dst, size_t len);
void UART_break(int unit_num, uint32_t interval);
void UART_flush(int unit_num);
void UART_clear_rx_buffer(int unit_num);
void UART_clear_tx_buffer(int unit_num);

#ifdef __cplusplus
}
#endif

#endif /* UART_DEFINED_H_ */
