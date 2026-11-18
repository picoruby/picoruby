#include <stdint.h>

#include "nrf_gpio.h"

#include "../../include/gpio.h"

/*
 * "P0.13" / "P1.02", the way the datasheet, the schematics and the board
 * silkscreen all spell a pin. Worth parsing here where it is not worth it
 * on RP2040: there GP7 is pin 7 and the string adds nothing, whereas
 * nRF52 flattens two ports into one 0..47 range, so P1.02 is pin 34 and
 * nobody reading a schematic can see that.
 *
 * '_' is accepted beside '.' so the symbol form needs no quoting:
 * GPIO.new(:P1_02) rather than GPIO.new(:"P1.02").
 */
int
GPIO_pin_num_from_char(const uint8_t *str)
{
  if (str == NULL) {
    return -1;
  }
  if (str[0] != 'P' && str[0] != 'p') {
    return -1;
  }

  uint8_t port = str[1];
  if (port != '0' && port != '1') {
    return -1;
  }

  /* Require the separator: without it "P012" could be read as P0.12 or
     as P01 followed by junk, and guessing is worse than refusing. */
  if (str[2] != '.' && str[2] != '_') {
    return -1;
  }

  const uint8_t *digits = &str[3];
  if (digits[0] < '0' || '9' < digits[0]) {
    return -1;
  }
  int number = digits[0] - '0';
  int consumed = 1;
  if ('0' <= digits[1] && digits[1] <= '9') {
    number = number * 10 + (digits[1] - '0');
    consumed = 2;
  }
  if (digits[consumed] != '\0') {
    return -1;  /* trailing characters */
  }

  if (port == '0') {
    return (number < P0_PIN_NUM) ? number : -1;
  }
  return (number < P1_PIN_NUM) ? (P0_PIN_NUM + number) : -1;
}

void
GPIO_init(uint8_t pin)
{
  nrf_gpio_cfg_default(pin);
}

void
GPIO_set_dir(uint8_t pin, uint8_t dir)
{
  switch (dir) {
    case IN:
      nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_NOPULL);
      break;
    case OUT:
      nrf_gpio_cfg_output(pin);
      break;
    case HIGH_Z:
      nrf_gpio_cfg(pin,
        NRF_GPIO_PIN_DIR_INPUT,
        NRF_GPIO_PIN_INPUT_DISCONNECT,
        NRF_GPIO_PIN_NOPULL,
        NRF_GPIO_PIN_S0S1,
        NRF_GPIO_PIN_NOSENSE);
      break;
  }
}

void
GPIO_open_drain(uint8_t pin)
{
  (void)pin;
}

void
GPIO_pull_up(uint8_t pin)
{
  nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLUP);
}

void
GPIO_pull_down(uint8_t pin)
{
  nrf_gpio_cfg_input(pin, NRF_GPIO_PIN_PULLDOWN);
}

int
GPIO_read(uint8_t pin)
{
  return nrf_gpio_pin_read(pin);
}

void
GPIO_write(uint8_t pin, uint8_t val)
{
  nrf_gpio_pin_write(pin, val ? 1u : 0u);
}

/*
 * Not supported, and there is nothing to support. PIN_CNF carries DIR,
 * INPUT, PULL, DRIVE and SENSE -- no function field. A peripheral claims
 * a pin from its own side instead, by writing the pin number into its
 * PSEL register (UARTE.PSEL.TXD, SPIM.PSEL.SCK, ...), so routing belongs
 * to the peripheral's port and GPIO::ALT has no nRF52 meaning.
 */
void
GPIO_set_function(uint8_t pin, uint8_t function)
{
  (void)pin;
  (void)function;
}
