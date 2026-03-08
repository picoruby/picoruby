/*
 * MQTT mruby bindings (placeholder implementation)
 *
 * This is a placeholder implementation for mruby compatibility.
 * The actual MQTT functionality is implemented in mrubyc/mqtt.c and ports/rp2040/mqtt.c
 */

#include <mruby.h>
#include "../../include/mqtt.h"

void
mrb_picoruby_mqtt_lwip_gem_init(mrb_state* mrb)
{
  // Placeholder - MQTT functionality is not supported in mruby builds
  // All implementation is in mrubyc/mqtt.c for mrubyc builds
}

void
mrb_picoruby_mqtt_lwip_gem_final(mrb_state* mrb)
{
  // Placeholder cleanup
}