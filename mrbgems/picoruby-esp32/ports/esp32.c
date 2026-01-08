#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "ESP32_WIFI";

static bool wifi_initialized = false;
static bool wifi_connected = false;
static EventGroupHandle_t wifi_event_group = NULL;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void
wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "WiFi started");
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    ESP_LOGI(TAG, "WiFi disconnected");
    wifi_connected = false;
    if (wifi_event_group) {
      xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    wifi_connected = true;
    if (wifi_event_group) {
      xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
  }
}

static void
ESP32_WIFI_log_level_set(esp_log_level_t level)
{
  esp_log_level_set("wifi", level);
  esp_log_level_set("wifi_init", level);
  esp_log_level_set("phy_init", level);
  esp_log_level_set("pp", level);
  esp_log_level_set("net80211", level);
  esp_log_level_set("esp_netif_handlers", level);
}

int
ESP32_WIFI_init()
{
  if (wifi_initialized) {
    ESP_LOGI(TAG, "WiFi already initialized");
    return 0;
  }

  ESP32_WIFI_log_level_set(ESP_LOG_WARN);

  esp_err_t ret = esp_netif_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
    return -1;
  }

  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
    return -1;
  }

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ret = esp_wifi_init(&cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
    return -1;
  }

  ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
    return -1;
  }

  ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
    return -1;
  }

  ret = esp_wifi_set_mode(WIFI_MODE_STA);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(ret));
    return -1;
  }

  wifi_initialized = true;
  ESP_LOGI(TAG, "WiFi initialized successfully");
  return 0;
}

int
ESP32_WIFI_initialized()
{
  return wifi_initialized ? 1 : 0;
}

int
ESP32_WIFI_connect_timeout(const char* ssid, const char* password, int auth, int timeout_ms)
{
  if (wifi_event_group == NULL) {
    wifi_event_group = xEventGroupCreate();
  }
  xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

  wifi_config_t wifi_config = {0};
  strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
  strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
  wifi_config.sta.threshold.authmode = auth;

  esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(ret));
    return -1;
  }

  ret = esp_wifi_start();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
    return -1;
  }

  ret = esp_wifi_connect();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_connect failed: %s", esp_err_to_name(ret));
    return -1;
  }

  // Wait for connection (with timeout)
  EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                         WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                         pdFALSE,
                                         pdFALSE,
                                         pdMS_TO_TICKS(timeout_ms));

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "Connected to AP SSID:%s", ssid);
    return 0; // Success
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGE(TAG, "Failed to connect to SSID:%s", ssid);
    return -1; // Failure
  } else {
    ESP_LOGE(TAG, "Connection timeout for SSID:%s", ssid);
    return -2; // Timeout
  }
}

int
ESP32_WIFI_disconnect()
{
  esp_wifi_disconnect();
  esp_wifi_stop();
  wifi_connected = false;
  ESP_LOGI(TAG, "WiFi disconnected");
  return 0;
}

int
ESP32_WIFI_tcpip_link_status()
{
  if (!wifi_initialized) {
    return 5; // LINK_NONET
  }

  if (wifi_connected) {
    return 3; // LINK_UP
  }

  wifi_ap_record_t ap_info;
  esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);

  if (ret == ESP_OK) {
    // Connected to AP, but no IP address yet
    return 2; // LINK_NOIP
  } else {
    // Not connected to an AP
    return 0; // LINK_DOWN
  }
}