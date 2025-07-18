#include "pico/stdlib.h"
#include "../../include/mqtt.h"

void mqtt_sleep_ms(int ms) {
    sleep_ms(ms);
}

bool mqtt_platform_init(void) {
    // Any RP2040-specific initialization can go here
    return true;
}

void mqtt_platform_cleanup(void) {
    // Any RP2040-specific cleanup can go here
}
//
// Network status check
bool mqtt_network_available(void) {
    return true; // For now, assume network is always available
}
