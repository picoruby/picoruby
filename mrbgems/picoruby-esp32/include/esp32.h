#ifndef ESP32_DEFINED_H_
#define ESP32_DEFINED_H_

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(USE_WIFI)
int ESP32_WIFI_init();
int ESP32_WIFI_initialized();
int ESP32_WIFI_connect_timeout(const char* ssid, const char* password, int auth, int timeout_ms);
int ESP32_WIFI_disconnect();
int ESP32_WIFI_tcpip_link_status();
bool ESP32_WIFI_dhcp_supplied(void);
const char *ESP32_WIFI_ipv4_address(char *buf, size_t buflen);
const char *ESP32_WIFI_ipv4_netmask(char *buf, size_t buflen);
const char *ESP32_WIFI_ipv4_gateway(char *buf, size_t buflen);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ESP32_DEFINED_H_ */
