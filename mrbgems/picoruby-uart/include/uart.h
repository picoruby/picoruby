#ifndef UART_DEFINED_H_
#define UART_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PARITY_NONE   0
#define PARITY_EVEN   1
#define PARITY_ODD    2
#define FLOW_CONTROL_NONE       0
#define FLOW_CONTROL_RTS_CTS    1

#define PICORUBY_UART_RP2040_UART0      0
#define PICORUBY_UART_RP2040_UART1      1

typedef enum {
 ERROR_NONE              =  0,
 ERROR_INVALID_UNIT      = -1,
} uart_status_t;

int UART_unit_name_to_unit_num(const char *unit_name);
uint32_t UART_init(int unit_num, uint32_t baudrate, uint32_t txd_pin, uint32_t rxd_pin);
uint32_t UART_set_baudrate(int unit_num, uint32_t baudrate);
void UART_set_flow_control(int unit_num, bool cts, bool rts);
void UART_set_format(int unit_num, uint32_t data_bits, uint32_t stop_bits, uint8_t parity);
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

