#include "include/common.h"
#include "mrubyc.h"
#include "vm.h"
#include "value.h"
#include "class.h"
#include "error.h"
#include "symbol.h"
#include "../../include/mqtt.h"
#include "lwipopts.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"
#include <string.h>
#include <mrc_compile.h>

#define MQTT_CONNECT     0x10
#define MQTT_CONNACK     0x20
#define MQTT_PUBLISH     0x30
#define MQTT_SUBSCRIBE   0x82
#define MQTT_SUBACK      0x90
#define MQTT_PINGREQ     0xC0
#define MQTT_PINGRESP    0xD0
#define MQTT_DISCONNECT  0xE0

#define MQTT_MESSAGE_QUEUE_SIZE 8

typedef struct {
  int state;
  struct altcp_pcb *pcb;
  mrbc_value client_id;
  mrbc_vm *vm;
  struct altcp_tls_config *tls_config;
  bool connected;
  uint32_t last_ping_time;
  uint32_t last_message_time;
  uint16_t keep_alive_interval;
  mrbc_value self;
} mqtt_client_state_t;

static mqtt_client_state_t *current_mqtt_state = NULL;

static err_t mqtt_client_poll(void *arg, struct altcp_pcb *pcb);
static void mqtt_client_update_message_time(mqtt_client_state_t *mqtt);
static void mqtt_client_send_pingreq(mqtt_client_state_t *mqtt);
static err_t mqtt_client_close(void *arg);

static void encode_remaining_length(uint8_t *buf, int length) {
  int pos = 0;
  do {
    uint8_t byte = length % 128;
    length /= 128;
    if (length > 0) {
      byte |= 0x80;
    }
    buf[pos++] = byte;
  } while (length > 0);
}

static err_t mqtt_client_close(void *arg) {
  mqtt_client_state_t *mqtt = (mqtt_client_state_t*)arg;
  if (!mqtt) return ERR_OK;

  if (mqtt->pcb != NULL) {
    altcp_arg(mqtt->pcb, NULL);
    altcp_recv(mqtt->pcb, NULL);
    altcp_err(mqtt->pcb, NULL);
    altcp_poll(mqtt->pcb, NULL, 0);
    altcp_sent(mqtt->pcb, NULL);
    altcp_close(mqtt->pcb);
    mqtt->pcb = NULL;
  }

  if (mqtt->tls_config) {
    altcp_tls_free_config(mqtt->tls_config);
    mqtt->tls_config = NULL;
  }

  mqtt->connected = false;
  return ERR_OK;
}

static err_t mqtt_client_connected(void *arg, struct altcp_pcb *pcb, err_t err) {
  mqtt_client_state_t *mqtt = (mqtt_client_state_t*)arg;
  if (err != ERR_OK) {
    console_printf("MQTT: Connection callback error: %d\n", err);
    return mqtt_client_close(mqtt);
  }

  uint8_t packet[256];
  uint8_t *ptr = packet;

  *ptr++ = MQTT_CONNECT;
  ptr++;

  *ptr++ = 0; *ptr++ = 4; // Length of "MQTT"
  *ptr++ = 'M'; *ptr++ = 'Q'; *ptr++ = 'T'; *ptr++ = 'T';
  *ptr++ = 4; // Protocol version 4
  *ptr++ = 0x02; // Clean session
  *ptr++ = 0; *ptr++ = 60; // Keep alive 60 seconds

  const char *client_id = (const char*)mqtt->client_id.string->data;
  size_t client_id_len = strlen(client_id);
  *ptr++ = (client_id_len >> 8) & 0xFF;
  *ptr++ = client_id_len & 0xFF;
  memcpy(ptr, client_id, client_id_len);
  ptr += client_id_len;

  size_t remaining_length = ptr - packet - 2;
  encode_remaining_length(packet + 1, remaining_length);

  err_t result = altcp_write(pcb, packet, ptr - packet, TCP_WRITE_FLAG_COPY);
  if (result != ERR_OK) {
    return mqtt_client_close(mqtt);
  }

  result = altcp_output(pcb);
  if (result != ERR_OK) {
    return mqtt_client_close(mqtt);
  }

  mqtt->connected = true;

  return ERR_OK;
}

static err_t mqtt_client_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
  mqtt_client_state_t *mqtt = (mqtt_client_state_t*)arg;
  MQTT_callback();

  if (p == NULL) {
    return mqtt_client_close(mqtt);
  }

  if (p->len < 1) {
    altcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
  }

  uint8_t *data = (uint8_t*)p->payload;
  uint8_t packet_type = data[0] & 0xF0;

  switch(packet_type) {
    case MQTT_CONNACK:
      if (p->len >= 4) {
        console_printf("MQTT: Received CONNACK, return code: %d\n", data[3]);
        if (data[3] == 0) {
          mqtt->connected = true;
          console_printf("MQTT: Successfully connected to broker\n");
          mqtt_client_update_message_time(mqtt);
        } else {
          console_printf("MQTT: Connection rejected by broker\n");
          mqtt->connected = false;
        }
      }
      break;

    case MQTT_PUBLISH: {
      mqtt_client_update_message_time(mqtt);
      break;
    }

    case MQTT_PINGRESP:
      if (p->len == 2) {
        mqtt_client_update_message_time(mqtt);
      }
      break;

    default:
      mqtt_client_update_message_time(mqtt);
      break;
  }

  altcp_recved(pcb, p->tot_len);
  pbuf_free(p);
  return ERR_OK;
}

mrbc_value MQTTClient_connect(mrbc_vm *vm, mrbc_value *self, const char *host, int port, const char *client_id, bool use_tls, const char *ca_cert) {
  console_printf("MQTT: Starting connection to %s:%d\n", host, port);
  if (current_mqtt_state != NULL) {
    console_printf("MQTT: Closing existing connection\n");
    mqtt_client_close(current_mqtt_state);
    mrbc_free(vm, current_mqtt_state);
    current_mqtt_state = NULL;
  }

  mqtt_client_state_t *mqtt = (mqtt_client_state_t *)mrbc_alloc(vm, sizeof(mqtt_client_state_t));
  if (!mqtt) {
    console_printf("MQTT: Failed to allocate state\n");
    return mrbc_false_value();
  }
  memset(mqtt, 0, sizeof(mqtt_client_state_t));

  mqtt->vm = vm;
  mqtt->self = *self;
  mqtt->client_id = mrbc_string_new(vm, client_id, strlen(client_id));
  mqtt->keep_alive_interval = 60;
  mqtt->last_message_time = to_ms_since_boot(get_absolute_time()) / 1000;
  mqtt->last_ping_time = mqtt->last_message_time;

  if (use_tls) {
    struct altcp_tls_config *tls_config;
    if (ca_cert) {
      tls_config = altcp_tls_create_config_client((void*)ca_cert, strlen(ca_cert));
    } else {
      tls_config = altcp_tls_create_config_client(NULL, 0);
    }

    if (!tls_config) {
      console_printf("MQTT: Failed to create TLS config\n");
      mrbc_free(vm, mqtt);
      return mrbc_false_value();
    }

    mqtt->tls_config = tls_config;
    mqtt->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_V4);
  } else {
    mqtt->pcb = altcp_new(NULL);
  }

  if (mqtt->pcb == NULL) {
    if (use_tls && mqtt->tls_config) {
      altcp_tls_free_config(mqtt->tls_config);
    }
    mrbc_free(vm, mqtt);
    return mrbc_false_value();
  }

  altcp_arg(mqtt->pcb, mqtt);
  altcp_recv(mqtt->pcb, mqtt_client_recv);

  ip_addr_t remote_addr;
  ip4_addr_set_zero(&remote_addr);
  err_t err = Net_get_ip(host, &remote_addr);
  if (err != ERR_OK) {
    mqtt_client_close(mqtt);
    mrbc_free(vm, mqtt);
    return mrbc_false_value();
  }

  console_printf("MQTT: Connecting to IP: %s\n", ipaddr_ntoa(&remote_addr));
  err = altcp_connect(mqtt->pcb, &remote_addr, port, mqtt_client_connected);
  if (err != ERR_OK) {
    console_printf("MQTT: Connection failed: %d\n", err);
    mqtt_client_close(mqtt);
    mrbc_free(vm, mqtt);
    return mrbc_false_value();
  }

  altcp_poll(mqtt->pcb, mqtt_client_poll, 10);

  current_mqtt_state = mqtt;
  return mrbc_true_value();
}

mrbc_value MQTTClient_publish(mrbc_vm *vm, mrbc_value *payload, const char *topic) {
  if (!current_mqtt_state || !current_mqtt_state->connected) {
    console_printf("MQTT: Not connected\n");
    return mrbc_false_value();
  }

  console_printf("MQTT: Publish => topic=\"%s\"\n", topic);

  size_t topic_len = strlen(topic);
  size_t payload_len = payload->string->size;
  size_t remaining_length = 2 + topic_len + payload_len;

  uint8_t fixed_header[5];
  fixed_header[0] = MQTT_PUBLISH;

  int remaining_length_bytes = 0;
  size_t tmp_len = remaining_length;
  do {
    uint8_t byte = tmp_len % 128;
    tmp_len /= 128;
    if (tmp_len > 0) byte |= 0x80;
    fixed_header[1 + remaining_length_bytes++] = byte;
  } while (tmp_len > 0);

  err_t err = altcp_write(current_mqtt_state->pcb, fixed_header, 1 + remaining_length_bytes, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to write fixed header, err: %d\n", err);
    return mrbc_false_value();
  }

  uint8_t topic_len_bytes[2] = {(topic_len >> 8) & 0xFF, topic_len & 0xFF};
  err = altcp_write(current_mqtt_state->pcb, topic_len_bytes, 2, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to write topic length, err: %d\n", err);
    return mrbc_false_value();
  }

  err = altcp_write(current_mqtt_state->pcb, topic, topic_len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to write topic, err: %d\n", err);
    return mrbc_false_value();
  }

  err = altcp_write(current_mqtt_state->pcb, payload->string->data, payload_len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to write payload, err: %d\n", err);
    return mrbc_false_value();
  }

  err = altcp_output(current_mqtt_state->pcb);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to flush output, err: %d\n", err);
    return mrbc_false_value();
  }

  mqtt_client_update_message_time(current_mqtt_state);
  console_printf("MQTT: Publish completed successfully\n");
  return mrbc_true_value();
}

static err_t mqtt_client_poll(void *arg, struct altcp_pcb *pcb) {
  mqtt_client_state_t *mqtt = (mqtt_client_state_t*)arg;
  if (!mqtt || !mqtt->connected) {
    return ERR_OK;
  }

  uint32_t current_time = to_ms_since_boot(get_absolute_time()) / 1000;
  uint32_t elapsed_time = current_time - mqtt->last_message_time;

  if (elapsed_time >= (mqtt->keep_alive_interval * 500)) {
    mqtt_client_send_pingreq(mqtt);
    mqtt->last_ping_time = current_time;
  }

  if (elapsed_time >= (mqtt->keep_alive_interval * 1500)) {
    console_printf("MQTT: Keep Alive timeout\n");
    mqtt_client_close(mqtt);
  }

  return ERR_OK;
}

static void mqtt_client_send_pingreq(mqtt_client_state_t *mqtt) {
  if (!mqtt || !mqtt->connected) return;

  console_printf("MQTT: Sending PINGREQ\n");
  uint8_t pingreq_packet[2] = {
    MQTT_PINGREQ,
    0
  };

  err_t err = altcp_write(mqtt->pcb, pingreq_packet, 2, TCP_WRITE_FLAG_COPY);
  if (err == ERR_OK) {
    err = altcp_output(mqtt->pcb);
    if (err == ERR_OK) {
        console_printf("MQTT: Sent PINGREQ OK\n");
    } else {
      console_printf("MQTT: Failed to send PINGREQ, err: %d\n", err);
      mqtt_client_close(mqtt);
    }
  } else {
    console_printf("MQTT: Failed to write PINGREQ, err: %d\n", err);
    mqtt_client_close(mqtt);
  }
}

static void mqtt_client_update_message_time(mqtt_client_state_t *mqtt) {
  if (mqtt) {
    mqtt->last_message_time = to_ms_since_boot(get_absolute_time()) / 1000;
  }
}

mrbc_value MQTTClient_subscribe(mrbc_vm *vm, const char *topic) {
  if (!current_mqtt_state || !current_mqtt_state->connected) {
    console_printf("MQTT: subscribe => not connected\n");
    return mrbc_false_value();
  }

  console_printf("MQTT: subscribe => topic=\"%s\"\n", topic);

  size_t topic_len = strlen(topic);

  size_t remaining_length = 2 + 2 + topic_len + 1; // packet ID(2) + topic length(2) + topic + QoS(1)

  uint8_t fixed_header[5];
  fixed_header[0] = MQTT_SUBSCRIBE | 0x02;

  int remaining_length_bytes = 0;
  size_t tmp_len = remaining_length;
  do {
    uint8_t byte = tmp_len % 128;
    tmp_len /= 128;
    if (tmp_len > 0) byte |= 0x80;
    fixed_header[1 + remaining_length_bytes++] = byte;
  } while (tmp_len > 0);

  err_t err = altcp_write(current_mqtt_state->pcb, fixed_header, 1 + remaining_length_bytes, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: subscribe => write fixed_header err=%d\n", err);
    return mrbc_false_value();
  }

  uint8_t packet_id[2] = {0, 1};
  err = altcp_write(current_mqtt_state->pcb, packet_id, 2, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: subscribe => write packet_id err=%d\n", err);
    return mrbc_false_value();
  }

  uint8_t topic_len_bytes[2] = {(topic_len >> 8) & 0xFF, topic_len & 0xFF};
  err = altcp_write(current_mqtt_state->pcb, topic_len_bytes, 2, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: subscribe => write topic_len err=%d\n", err);
    return mrbc_false_value();
  }

  err = altcp_write(current_mqtt_state->pcb, topic, topic_len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: subscribe => write topic err=%d\n", err);
    return mrbc_false_value();
  }

  uint8_t qos = 0;
  err = altcp_write(current_mqtt_state->pcb, &qos, 1, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: subscribe => write QoS err=%d\n", err);
    return mrbc_false_value();
  }

  err = altcp_output(current_mqtt_state->pcb);
  if (err != ERR_OK) {
    console_printf("MQTT: subscribe => flush err=%d\n", err);
    return mrbc_false_value();
  }

  console_printf("MQTT: Subscribed to topic: %s with QoS: 0\n", topic);
  return mrbc_true_value();
}

mrbc_value MQTTClient_disconnect(mrbc_vm *vm) {
  if (!current_mqtt_state || !current_mqtt_state->connected) {
    console_printf("MQTT: disconnect => not connected\n");
    return mrbc_false_value();
  }

  uint8_t disconnect_packet = MQTT_DISCONNECT;
  err_t err = altcp_write(current_mqtt_state->pcb, &disconnect_packet, 1, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    return mrbc_false_value();
  }

  err = altcp_output(current_mqtt_state->pcb);
  if (err != ERR_OK) {
    return mrbc_false_value();
  }

  mqtt_client_close(current_mqtt_state);
  mrbc_free(vm, current_mqtt_state);
  current_mqtt_state = NULL;

  return mrbc_true_value();
}
