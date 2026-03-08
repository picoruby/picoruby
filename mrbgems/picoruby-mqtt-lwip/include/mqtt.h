#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>

#include "picoruby.h"

// lwIP headers are only needed for the actual implementation, not for mruby builds
#if defined(PICORB_VM_MRUBYC) && !defined(DISABLE_MRUBY)
#include <mrubyc.h>
#include "lwip/apps/mqtt.h"
#endif

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
#if defined(PICORB_VM_MRUBYC) && !defined(DISABLE_MRUBY)
  mqtt_client_t *client;
#else
  void *client;  // Placeholder for mruby builds
#endif
  mqtt_fsm_state_t fsm_state;

  char topic_to_sub[MQTT_TOPIC_MAX_LEN];
  char topic_to_pub[MQTT_TOPIC_MAX_LEN];
  char payload_to_pub[MQTT_PAYLOAD_MAX_LEN];
  int payload_to_pub_len;

  char recv_topic[MQTT_TOPIC_MAX_LEN];
  char recv_payload[MQTT_PAYLOAD_MAX_LEN];
  int recv_payload_len;
  bool message_arrived;

#if defined(PICORB_VM_MRUBY)
  // No callback support in mruby placeholder
#elif defined(PICORB_VM_MRUBYC) && !defined(DISABLE_MRUBY)
  mrbc_vm *vm;
  mrbc_value callback_proc;
#endif
} mqtt_context_t;

int MQTT_connect_impl(const char *host, int port, const char *client_id);
int MQTT_subscribe_impl(const char *topic);
int MQTT_publish_impl(const char *topic, const char *payload, int len);
void MQTT_disconnect_impl(void);
int MQTT_get_message_impl(char **topic, char **payload);

void MQTT_init_context(void *vm);
void MQTT_set_callback(void *proc);

#if defined(PICORB_VM_MRUBY)
void mrb_picoruby_mqtt_lwip_gem_init(mrb_state* mrb);
void mrb_picoruby_mqtt_lwip_gem_final(mrb_state* mrb);
#elif defined(PICORB_VM_MRUBYC)
void mrbc_mqtt_lwip_init(mrbc_vm *vm);
#endif

#ifdef __cplusplus
}
#endif

#endif
