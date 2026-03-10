#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>

#include "picoruby.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_TOPIC_MAX_LEN 64
#define MQTT_PAYLOAD_MAX_LEN 256

typedef enum {
  MQTT_STATE_IDLE,
  MQTT_STATE_CONNECTING,
  MQTT_STATE_CONNACK_WAIT,
  MQTT_STATE_ACTIVE,
  MQTT_STATE_SUBSCRIBING,
  MQTT_STATE_PUBLISHING,
  MQTT_STATE_DISCONNECTING,
  MQTT_STATE_ERROR,
  MQTT_STATE_TIMEOUT
} mqtt_fsm_state_t;

typedef struct {
  void *client;  // Platform-specific client pointer
  mqtt_fsm_state_t fsm_state;

  char topic_to_sub[MQTT_TOPIC_MAX_LEN];
  char topic_to_pub[MQTT_TOPIC_MAX_LEN];
  char payload_to_pub[MQTT_PAYLOAD_MAX_LEN];
  int payload_to_pub_len;

  char recv_topic[MQTT_TOPIC_MAX_LEN];
  char recv_payload[MQTT_PAYLOAD_MAX_LEN];
  int recv_payload_len;
  bool message_arrived;

  void *vm;           // VM pointer (platform-specific)
  void *callback_proc; // Callback procedure (platform-specific)
} mqtt_context_t;

int MQTT_connect_impl(const char *host, int port, const char *client_id);
int MQTT_is_connected_impl(void);
int MQTT_subscribe_impl(const char *topic);
int MQTT_publish_impl(const char *topic, const char *payload, int len);
void MQTT_disconnect_impl(void);
int MQTT_get_message_impl(char **topic, char **payload);

void MQTT_init_context(void *vm);
void MQTT_set_callback(void *proc);

void mrb_picoruby_mqtt_lwip_gem_init(void* mrb);
void mrb_picoruby_mqtt_lwip_gem_final(void* mrb);
void mrbc_mqtt_lwip_init(void *vm);

#ifdef __cplusplus
}
#endif

#endif
