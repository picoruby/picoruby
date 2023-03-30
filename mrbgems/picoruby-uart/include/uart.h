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

int UART_unit_name_to_unit_num(const char *unit_name);
uint32_t UART_init(uint8_t *uart, uint32_t baudrate);
uint32_t UART_set_barudrate(uint8_t *uart, uint32_t baudrate);
void UART_set_hw_flow(uint8_t *uart, bool cts, bool rts);
void UART_set_format(uint8_t *uart, uint32_t data_bits, uint32_t stop_bits, uint8_t parity);
bool UART_is_wriable(uint8_t *uart);
void UART_putc_raw(uint8_t *uart, uint8_t c);
bool UART_is_readable(uint8_t *uart);
uint8_t UART_getc(uint8_t *uart);
void UART_set_break(uint8_t *uart, bool en);
void UART_tx_wait_blocking(uint8_t *uart);

#ifdef __cplusplus
}
#endif

#endif /* UART_DEFINED_H_ */

