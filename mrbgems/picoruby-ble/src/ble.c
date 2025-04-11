#include <stdbool.h>
#include "picoruby.h"
#include "../include/ble.h"
#include "../include/ble_central.h"

#include "../../picoruby-machine/include/machine.h"

/*
 * Workaround: To avoid deadlock
 * TODO: Maybe we need a critical section instead of these simple mutex
 */
static bool packet_mutex = false;
static bool write_values_mutex = false;
static bool heatbeat_flag = false;
static bool packet_flag = false;

static uint8_t *packet = NULL;
static uint16_t packet_size = 0;

#if defined(PICORB_VM_MRUBY)

#include "mruby/ble.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/ble.c"

#endif
