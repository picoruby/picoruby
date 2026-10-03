#include "soc/soc_caps.h"

#include "../../include/pulse_counter.h"

#if SOC_PCNT_SUPPORTED

#include "driver/pulse_cnt.h"
#include "driver/gpio.h"

/* Hardware counter is 16-bit. With accum_count enabled, the driver adds
 * the limit to an internal accumulator each time a watch point at the
 * limit is hit, so the reported count extends to int. */
#define PULSE_COUNTER_HIGH_LIMIT  (30000)
#define PULSE_COUNTER_LOW_LIMIT   (-30000)
#define PULSE_COUNTER_MAX_UNITS   (8)

static pcnt_unit_handle_t pcnt_units[PULSE_COUNTER_MAX_UNITS];

static int
find_free_slot(void)
{
  for (int i = 0; i < PULSE_COUNTER_MAX_UNITS; i++) {
    if (pcnt_units[i] == NULL) return i;
  }
  return -1;
}

int
PulseCounter_init(uint32_t pin_a, uint32_t pin_b, uint32_t glitch_ns, bool pull_up)
{
  if (pin_a >= GPIO_NUM_MAX || pin_b >= GPIO_NUM_MAX) return -1;

  int slot = find_free_slot();
  if (slot < 0) return -1;

  pcnt_unit_config_t unit_config = {
    .low_limit = PULSE_COUNTER_LOW_LIMIT,
    .high_limit = PULSE_COUNTER_HIGH_LIMIT,
    .flags.accum_count = 1,
  };
  pcnt_unit_handle_t unit = NULL;
  pcnt_channel_handle_t chan_a = NULL;
  pcnt_channel_handle_t chan_b = NULL;
  if (pcnt_new_unit(&unit_config, &unit) != ESP_OK) return -1;

  if (0 < glitch_ns) {
    pcnt_glitch_filter_config_t filter_config = {
      .max_glitch_ns = glitch_ns,
    };
    if (pcnt_unit_set_glitch_filter(unit, &filter_config) != ESP_OK) goto error;
  }

  /* x4 quadrature decoding: each channel counts edges of one phase
   * and uses the level of the other phase for the direction. */
  pcnt_chan_config_t chan_a_config = {
    .edge_gpio_num = pin_a,
    .level_gpio_num = pin_b,
  };
  if (pcnt_new_channel(unit, &chan_a_config, &chan_a) != ESP_OK) goto error;

  pcnt_chan_config_t chan_b_config = {
    .edge_gpio_num = pin_b,
    .level_gpio_num = pin_a,
  };
  if (pcnt_new_channel(unit, &chan_b_config, &chan_b) != ESP_OK) goto error;

  pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

  /* Set explicitly: whether the driver enables pull-ups differs
   * between ESP-IDF versions. */
  if (pull_up) {
    gpio_pullup_en(pin_a);
    gpio_pullup_en(pin_b);
  } else {
    gpio_pullup_dis(pin_a);
    gpio_pullup_dis(pin_b);
  }

  if (pcnt_unit_add_watch_point(unit, PULSE_COUNTER_HIGH_LIMIT) != ESP_OK) goto error;
  if (pcnt_unit_add_watch_point(unit, PULSE_COUNTER_LOW_LIMIT) != ESP_OK) goto error;

  if (pcnt_unit_enable(unit) != ESP_OK) goto error;
  if (pcnt_unit_clear_count(unit) != ESP_OK) goto error;
  if (pcnt_unit_start(unit) != ESP_OK) goto error;

  pcnt_units[slot] = unit;
  return slot;

error:
  if (chan_a) pcnt_del_channel(chan_a);
  if (chan_b) pcnt_del_channel(chan_b);
  pcnt_del_unit(unit);
  return -1;
}

int
PulseCounter_get_count(int unit_id, int32_t *count)
{
  if (unit_id < 0 || PULSE_COUNTER_MAX_UNITS <= unit_id || pcnt_units[unit_id] == NULL) return -1;

  int value;
  if (pcnt_unit_get_count(pcnt_units[unit_id], &value) != ESP_OK) return -1;
  *count = (int32_t)value;
  return 0;
}

int
PulseCounter_clear(int unit_id)
{
  if (unit_id < 0 || PULSE_COUNTER_MAX_UNITS <= unit_id || pcnt_units[unit_id] == NULL) return -1;

  if (pcnt_unit_clear_count(pcnt_units[unit_id]) != ESP_OK) return -1;
  return 0;
}

#else /* SOC_PCNT_SUPPORTED */

/* e.g. ESP32-C2/C3 have no PCNT peripheral */

int
PulseCounter_init(uint32_t pin_a, uint32_t pin_b, uint32_t glitch_ns, bool pull_up)
{
  return -1;
}

int
PulseCounter_get_count(int unit_id, int32_t *count)
{
  return -1;
}

int
PulseCounter_clear(int unit_id)
{
  return -1;
}

#endif /* SOC_PCNT_SUPPORTED */
