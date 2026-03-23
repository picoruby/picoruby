#ifndef ESP32_DEFINED_H_
#define ESP32_DEFINED_H_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_ESP_WIFI_ENABLED)

int ESP32_WIFI_init();
int ESP32_WIFI_initialized();
int ESP32_WIFI_connect_timeout(const char* ssid, const char* password, int auth, int timeout_ms);
int ESP32_WIFI_disconnect();
int ESP32_WIFI_tcpip_link_status();

#endif

#ifdef __cplusplus
}
#endif

#endif /* ESP32_DEFINED_H_ */
