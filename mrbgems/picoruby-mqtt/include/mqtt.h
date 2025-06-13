#ifndef MQTT_DEFINED_H_
#define MQTT_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>
#include "picoruby.h"
#include "net.h"

// WIP: TLS/SSL support is planned but not yet implemented.
// Dependencies (picoruby-mbedtls) are added in mrbgem.rake.

// MQTT packet types
#define MQTT_CONNECT     1
#define MQTT_CONNACK     2
#define MQTT_PUBLISH     3
#define MQTT_PUBACK      4
#define MQTT_SUBSCRIBE   8
#define MQTT_SUBACK      9
#define MQTT_DISCONNECT  14

// MQTT connection states
#define MQTT_STATE_DISCONNECTED  0
#define MQTT_STATE_CONNECTING    1
#define MQTT_STATE_CONNECTED     2
#define MQTT_STATE_DISCONNECTING 3

// MQTT client structure
typedef struct {
  const char *host;
  int port;
  const char *client_id;
  int state;
  struct altcp_pcb *pcb;
  picorb_state *vm;
  char *recv_buffer;
  size_t recv_buffer_len;
  
  // Packet queue
  uint8_t *packet_buffer;
  uint16_t packet_size;
  bool packet_available;
  bool packet_mutex;
  
  // Callback state tracking
  bool in_callback;  // Tracks if we're currently in a callback context
} mqtt_client_t;

#ifdef __cplusplus
extern "C" {
#endif

// Global MQTT client instance
extern mqtt_client_t *g_mqtt_client;

size_t encode_variable_length(size_t length, uint8_t *buf);

bool MQTT_connect(picorb_state *vm, const char *host, int port, const char *client_id);
bool MQTT_publish(picorb_state *vm, const char *payload, const char *topic);
bool MQTT_subscribe(picorb_state *vm, const char *topic);
bool MQTT_disconnect(picorb_state *vm);

void MQTT_push_event(uint8_t *data, uint16_t size);
bool MQTT_pop_event(uint8_t **data, uint16_t *size);

void MQTT_platform_init(void);
void MQTT_platform_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif
