#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#include "../../include/pio.h"

static PIO
pio_instance(uint8_t pio_num)
{
  switch (pio_num) {
    case PICORB_PIO0: return pio0;
    case PICORB_PIO1: return pio1;
    default: return NULL;
  }
}

pio_error_t
PIO_init(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return PIO_ERROR_INVALID_PIO;
  if (3 < config->sm_num) return PIO_ERROR_INVALID_SM;

  uint sm = config->sm_num;

  /* Build a pio_program_t to load */
  struct pio_program prog = {
    .instructions = config->instructions,
    .length = config->instruction_count,
    .origin = -1
  };

  if (!pio_can_add_program(pio, &prog)) {
    return PIO_ERROR_NO_SPACE;
  }
  config->program_offset = pio_add_program(pio, &prog);

  /* Get default config */
  pio_sm_config c = pio_get_default_sm_config();

  /* Set wrap */
  sm_config_set_wrap(&c,
    config->program_offset + config->wrap_target,
    config->program_offset + config->wrap
  );

  /* Side-set */
  uint side_set_total = config->side_set_count;
  if (config->side_set_optional) {
    side_set_total += 1; /* optional bit */
  }
  sm_config_set_sideset(&c, side_set_total, config->side_set_optional, false);

  /* OUT pins */
  if (0 <= config->out_pins) {
    sm_config_set_out_pins(&c, config->out_pins, config->out_pin_count);
    for (uint i = 0; i < config->out_pin_count; i++) {
      pio_gpio_init(pio, config->out_pins + i);
      pio_sm_set_consecutive_pindirs(pio, sm, config->out_pins + i, 1, true);
    }
  }

  /* SET pins */
  if (0 <= config->set_pins) {
    sm_config_set_set_pins(&c, config->set_pins, config->set_pin_count);
    for (uint i = 0; i < config->set_pin_count; i++) {
      pio_gpio_init(pio, config->set_pins + i);
      pio_sm_set_consecutive_pindirs(pio, sm, config->set_pins + i, 1, true);
    }
  }

  /* IN pins */
  if (0 <= config->in_pins) {
    sm_config_set_in_pins(&c, config->in_pins);
  }

  /* Side-set pins */
  if (0 <= config->sideset_pins) {
    sm_config_set_sideset_pins(&c, config->sideset_pins);
    for (uint i = 0; i < config->side_set_count; i++) {
      pio_gpio_init(pio, config->sideset_pins + i);
      pio_sm_set_consecutive_pindirs(pio, sm, config->sideset_pins + i, 1, true);
    }
  }

  /* JMP pin */
  if (0 <= config->jmp_pin) {
    sm_config_set_jmp_pin(&c, config->jmp_pin);
  }

  /* Output shift register */
  sm_config_set_out_shift(&c,
    config->out_shift_right,
    config->out_shift_autopull,
    config->out_shift_threshold
  );

  /* Input shift register */
  sm_config_set_in_shift(&c,
    config->in_shift_right,
    config->in_shift_autopush,
    config->in_shift_threshold
  );

  /* FIFO join */
  switch (config->fifo_join) {
    case PICORB_FIFO_JOIN_TX:
      sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
      break;
    case PICORB_FIFO_JOIN_RX:
      sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
      break;
    default:
      sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);
      break;
  }

  /* Clock divider */
  float clk_div = (float)clock_get_hz(clk_sys) / (float)config->freq;
  if (clk_div < 1.0f) clk_div = 1.0f;
  sm_config_set_clkdiv(&c, clk_div);

  /* Initialize and apply config */
  pio_sm_init(pio, sm, config->program_offset, &c);

  return PIO_ERROR_NONE;
}

void
PIO_set_enabled(pio_sm_config_t *config, bool enabled)
{
  PIO pio = pio_instance(config->pio_num);
  if (pio) {
    pio_sm_set_enabled(pio, config->sm_num, enabled);
  }
}

void
PIO_put_blocking(pio_sm_config_t *config, uint32_t value)
{
  PIO pio = pio_instance(config->pio_num);
  if (pio) {
    pio_sm_put_blocking(pio, config->sm_num, value);
  }
}

bool
PIO_put_nonblocking(pio_sm_config_t *config, uint32_t value)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return false;
  if (pio_sm_is_tx_fifo_full(pio, config->sm_num)) return false;
  pio_sm_put(pio, config->sm_num, value);
  return true;
}

uint32_t
PIO_get_blocking(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (pio) {
    return pio_sm_get_blocking(pio, config->sm_num);
  }
  return 0;
}

bool
PIO_get_nonblocking(pio_sm_config_t *config, uint32_t *value)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return false;
  if (pio_sm_is_rx_fifo_empty(pio, config->sm_num)) return false;
  *value = pio_sm_get(pio, config->sm_num);
  return true;
}

bool
PIO_tx_full(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return false;
  return pio_sm_is_tx_fifo_full(pio, config->sm_num);
}

bool
PIO_tx_empty(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return true;
  return pio_sm_is_tx_fifo_empty(pio, config->sm_num);
}

bool
PIO_rx_full(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return false;
  return pio_sm_is_rx_fifo_full(pio, config->sm_num);
}

bool
PIO_rx_empty(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return true;
  return pio_sm_is_rx_fifo_empty(pio, config->sm_num);
}

uint8_t
PIO_tx_level(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return 0;
  return pio_sm_get_tx_fifo_level(pio, config->sm_num);
}

uint8_t
PIO_rx_level(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (!pio) return 0;
  return pio_sm_get_rx_fifo_level(pio, config->sm_num);
}

void
PIO_clear_fifos(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (pio) {
    pio_sm_clear_fifos(pio, config->sm_num);
  }
}

void
PIO_drain_tx(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (pio) {
    pio_sm_drain_tx_fifo(pio, config->sm_num);
  }
}

void
PIO_restart(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (pio) {
    pio_sm_restart(pio, config->sm_num);
  }
}

void
PIO_exec(pio_sm_config_t *config, uint16_t instruction)
{
  PIO pio = pio_instance(config->pio_num);
  if (pio) {
    pio_sm_exec(pio, config->sm_num, instruction);
  }
}

void
PIO_deinit(pio_sm_config_t *config)
{
  PIO pio = pio_instance(config->pio_num);
  if (pio) {
    pio_sm_set_enabled(pio, config->sm_num, false);
    struct pio_program prog = {
      .instructions = config->instructions,
      .length = config->instruction_count,
      .origin = config->program_offset
    };
    pio_remove_program(pio, &prog, config->program_offset);
  }
}
