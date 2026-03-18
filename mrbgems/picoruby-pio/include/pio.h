#ifndef PIO_DEFINED_H_
#define PIO_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICORUBY_PIO0 0
#define PICORUBY_PIO1 1

#define PICORUBY_FIFO_JOIN_NONE 0
#define PICORUBY_FIFO_JOIN_TX   1
#define PICORUBY_FIFO_JOIN_RX   2

typedef enum {
  PIO_ERROR_NONE             =  0,
  PIO_ERROR_NO_SPACE         = -1,
  PIO_ERROR_INVALID_PIO      = -2,
  PIO_ERROR_INVALID_SM       = -3,
  PIO_ERROR_INVALID_PIN      = -4,
  PIO_ERROR_INIT_FAILED      = -5,
} pio_error_t;

typedef struct pio_sm_config {
  uint8_t  pio_num;
  uint8_t  sm_num;
  uint16_t *instructions;
  uint8_t  instruction_count;
  uint8_t  side_set_count;
  uint8_t  side_set_optional;
  uint8_t  wrap_target;
  uint8_t  wrap;
  uint32_t freq;
  int8_t   out_pins;
  uint8_t  out_pin_count;
  int8_t   set_pins;
  uint8_t  set_pin_count;
  int8_t   in_pins;
  int8_t   sideset_pins;
  int8_t   jmp_pin;
  uint8_t  out_shift_right;
  uint8_t  out_shift_autopull;
  uint8_t  out_shift_threshold;
  uint8_t  in_shift_right;
  uint8_t  in_shift_autopush;
  uint8_t  in_shift_threshold;
  uint8_t  fifo_join;
  uint8_t  program_offset;
  uint8_t  enabled;
} pio_sm_config_t;

pio_error_t PIO_init(pio_sm_config_t *config);
void PIO_set_enabled(pio_sm_config_t *config, bool enabled);
void PIO_put_blocking(pio_sm_config_t *config, uint32_t value);
bool PIO_put_nonblocking(pio_sm_config_t *config, uint32_t value);
uint32_t PIO_get_blocking(pio_sm_config_t *config);
bool PIO_get_nonblocking(pio_sm_config_t *config, uint32_t *value);
bool PIO_tx_full(pio_sm_config_t *config);
bool PIO_tx_empty(pio_sm_config_t *config);
bool PIO_rx_full(pio_sm_config_t *config);
bool PIO_rx_empty(pio_sm_config_t *config);
uint8_t PIO_tx_level(pio_sm_config_t *config);
uint8_t PIO_rx_level(pio_sm_config_t *config);
void PIO_clear_fifos(pio_sm_config_t *config);
void PIO_drain_tx(pio_sm_config_t *config);
void PIO_restart(pio_sm_config_t *config);
void PIO_exec(pio_sm_config_t *config, uint16_t instruction);
void PIO_deinit(pio_sm_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* PIO_DEFINED_H_ */
