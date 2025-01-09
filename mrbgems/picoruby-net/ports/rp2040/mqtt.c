#include "include/common.h"
#include "mrubyc.h"
#include "../../include/net.h"
#include "lwipopts.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/altcp.h"
#include "lwip/altcp_tls.h"
#include <string.h>

#define MQTT_CONNECT     0x10
#define MQTT_CONNACK     0x20
#define MQTT_PUBLISH     0x30
#define MQTT_SUBSCRIBE   0x82
#define MQTT_SUBACK      0x90
#define MQTT_PINGREQ     0xC0
#define MQTT_PINGRESP    0xD0
#define MQTT_DISCONNECT  0xE0

static void mqtt_client_poll(void *arg);

typedef struct {
  int state;
  struct altcp_pcb *pcb;
  mrbc_value client_id;
  mrbc_vm *vm;
  struct altcp_tls_config *tls_config;
  bool connected;
} mqtt_client_state_t;

static mqtt_client_state_t *current_mqtt_state = NULL;

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

  console_printf("MQTT: Connected callback started\n");

  // Build CONNECT packet
  uint8_t packet[256];
  uint8_t *ptr = packet;

  // Fixed header
  *ptr++ = MQTT_CONNECT;
  ptr++; // Length will be filled later

  // Variable header
  // Protocol name
  *ptr++ = 0; *ptr++ = 4; // Length of "MQTT"
  *ptr++ = 'M'; *ptr++ = 'Q'; *ptr++ = 'T'; *ptr++ = 'T';
  *ptr++ = 4; // Protocol version 4
  *ptr++ = 0x02; // Clean session
  *ptr++ = 0; *ptr++ = 60; // Keep alive 60 seconds

  // Payload - Client ID
  const char *client_id = (const char*)mqtt->client_id.string->data;
  size_t client_id_len = strlen(client_id);
  *ptr++ = (client_id_len >> 8) & 0xFF;
  *ptr++ = client_id_len & 0xFF;
  memcpy(ptr, client_id, client_id_len);
  ptr += client_id_len;

  // Fill in remaining length
  size_t remaining_length = ptr - packet - 2;
  encode_remaining_length(packet + 1, remaining_length);

  console_printf("MQTT: Sending CONNECT packet (total length: %d)\n", ptr - packet);

  // Send CONNECT packet
  err_t result = altcp_write(pcb, packet, ptr - packet, TCP_WRITE_FLAG_COPY);
  if (result != ERR_OK) {
    console_printf("MQTT: Failed to write CONNECT packet: %d\n", result);
    return mqtt_client_close(mqtt);
  }

  result = altcp_output(pcb);
  if (result != ERR_OK) {
    console_printf("MQTT: Failed to flush CONNECT packet: %d\n", result);
    return mqtt_client_close(mqtt);
  }

  mqtt->connected = true;
  console_printf("MQTT: Connect packet sent successfully\n");

  return ERR_OK;
}

// CONNACK 受信のためのコールバックも追加
static err_t mqtt_client_recv(void *arg, struct altcp_pcb *pcb, struct pbuf *p, err_t err) {
  mqtt_client_state_t *mqtt = (mqtt_client_state_t*)arg;
  
  if (p == NULL) {
    console_printf("MQTT: Connection closed by remote host\n");
    return mqtt_client_close(mqtt);
  }

  if (p->len >= 2) {
    uint8_t *data = (uint8_t*)p->payload;
    if (data[0] == MQTT_CONNACK) {
      console_printf("MQTT: Received CONNACK, return code: %d\n", data[3]);
      if (data[3] == 0) {
        mqtt->connected = true;
        console_printf("MQTT: Successfully connected to broker\n");
      } else {
        console_printf("MQTT: Connection rejected by broker\n");
        mqtt->connected = false;
      }
    }
  }

  altcp_recved(pcb, p->tot_len);
  pbuf_free(p);
  return ERR_OK;
}

mrbc_value MQTTClient_connect(mrbc_vm *vm, const char *host, int port, const char *client_id, bool use_tls) {
  console_printf("MQTT: Starting connection to %s:%d\n", host, port);
  if (current_mqtt_state != NULL) {
    console_printf("MQTT: Closing existing connection\n");
    mqtt_client_close(current_mqtt_state);
    mrbc_free(vm, current_mqtt_state);
  }

  mqtt_client_state_t *mqtt = (mqtt_client_state_t *)mrbc_alloc(vm, sizeof(mqtt_client_state_t));
  if (!mqtt) {
    console_printf("MQTT: Failed to allocate state\n");
    return mrbc_false_value();
  }

  memset(mqtt, 0, sizeof(mqtt_client_state_t));
  mqtt->vm = vm;
  mqtt->client_id = mrbc_string_new(vm, client_id, strlen(client_id));

  if (use_tls) {
    console_printf("MQTT: Setting up TLS\n");
    struct altcp_tls_config *tls_config = altcp_tls_create_config_client(NULL, 0);
    mqtt->tls_config = tls_config;
    mqtt->pcb = altcp_tls_new(tls_config, IPADDR_TYPE_V4);
  } else {
    mqtt->pcb = altcp_new(NULL);
  }

  if (mqtt->pcb == NULL) {
    console_printf("MQTT: Failed to create PCB\n");
    mrbc_free(vm, mqtt);
    return mrbc_false_value();
  }

  altcp_arg(mqtt->pcb, mqtt);
  altcp_recv(mqtt->pcb, mqtt_client_recv);

  ip_addr_t remote_addr;
  ip4_addr_set_zero(&remote_addr);
  err_t err = Net_get_ip(host, &remote_addr); // 修正済み
  if (err != ERR_OK) {
    console_printf("MQTT: DNS resolution failed: %d\n", err);
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

  // Add polling for keep-alive
  altcp_poll(mqtt->pcb, mqtt_client_poll, 10);

  current_mqtt_state = mqtt;
  console_printf("MQTT: Connection initiated\n");
  return mrbc_true_value();
}

static void mqtt_client_send_pingreq(mqtt_client_state_t *mqtt) {
  if (!mqtt || !mqtt->connected) return;

  uint8_t pingreq = MQTT_PINGREQ;
  err_t err = altcp_write(mqtt->pcb, &pingreq, 1, TCP_WRITE_FLAG_COPY);
  if (err == ERR_OK) {
    err = altcp_output(mqtt->pcb);
    console_printf("MQTT: Sent PINGREQ\n");
  }
}

mrbc_value MQTTClient_publish(mrbc_vm *vm, mrbc_value *payload, const char *topic) {
  if (!current_mqtt_state || !current_mqtt_state->connected) {
    console_printf("MQTT: Not connected\n");
    return mrbc_false_value();
  }

  // Build PUBLISH packet
  uint8_t header[4];
  header[0] = MQTT_PUBLISH;
  
  size_t topic_len = strlen(topic);
  size_t payload_len = payload->string->size;
  size_t remaining_length = 2 + topic_len + payload_len;

  console_printf("MQTT: Publishing - Topic: %s, Payload length: %d\n", topic, payload_len);

  encode_remaining_length(header + 1, remaining_length);

  // Send packet in parts
  err_t err = altcp_write(current_mqtt_state->pcb, header, sizeof(header), TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to write header, err: %d\n", err);
    return mrbc_false_value();
  };

  err = altcp_write(current_mqtt_state->pcb, topic, topic_len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to write topic, err: %d\n", err);
    return mrbc_false_value();
  };

  err = altcp_write(current_mqtt_state->pcb, payload->string->data, payload_len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to write payload, err: %d\n", err);
    return mrbc_false_value();
  };

  err = altcp_output(current_mqtt_state->pcb);
  if (err != ERR_OK) {
    console_printf("MQTT: Failed to flush output, err: %d\n", err);
    return mrbc_false_value();
  };

  console_printf("MQTT: Publish completed successfully\n");
  return mrbc_true_value();
}

// Keep Alive の実装
static void mqtt_client_poll(void *arg) {
  mqtt_client_state_t *mqtt = (mqtt_client_state_t*)arg;
  if (mqtt && mqtt->connected) {
    // Send PINGREQ every 30 seconds
    static uint32_t last_ping = 0;
    uint64_t now = time_us_64() / 1000;
    if (now - last_ping >= 30000) {
      mqtt_client_send_pingreq(mqtt);
      last_ping = now;
    }
  }
}

mrbc_value MQTTClient_subscribe(mrbc_vm *vm, const char *topic) {
  if (!current_mqtt_state || !current_mqtt_state->connected) {
    return mrbc_false_value();
  }

  // Build SUBSCRIBE packet
  uint8_t header[4];
  header[0] = MQTT_SUBSCRIBE;

  size_t topic_len = strlen(topic);
  size_t remaining_length = 2 + 2 + topic_len + 1; // packet ID + topic length + topic + QoS
  encode_remaining_length(header + 1, remaining_length);

  // Send packet in parts
  err_t err = altcp_write(current_mqtt_state->pcb, header, sizeof(header), TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) return mrbc_false_value();

  uint8_t packet_id[2] = {0, 1};
  err = altcp_write(current_mqtt_state->pcb, packet_id, 2, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) return mrbc_false_value();

  err = altcp_write(current_mqtt_state->pcb, topic, topic_len, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) return mrbc_false_value();

  uint8_t qos = 0;
  err = altcp_write(current_mqtt_state->pcb, &qos, 1, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) return mrbc_false_value();

  err = altcp_output(current_mqtt_state->pcb);
  if (err != ERR_OK) return mrbc_false_value();

  return mrbc_true_value();
}

mrbc_value MQTTClient_disconnect(mrbc_vm *vm) {
  if (!current_mqtt_state || !current_mqtt_state->connected) {
    return mrbc_false_value();
  }

  uint8_t disconnect_packet = MQTT_DISCONNECT;
  err_t err = altcp_write(current_mqtt_state->pcb, &disconnect_packet, 1, TCP_WRITE_FLAG_COPY);
  if (err != ERR_OK) return mrbc_false_value();

  err = altcp_output(current_mqtt_state->pcb);
  if (err != ERR_OK) return mrbc_false_value();

  mqtt_client_close(current_mqtt_state);
  mrbc_free(vm, current_mqtt_state);
  current_mqtt_state = NULL;

  return mrbc_true_value();
}
