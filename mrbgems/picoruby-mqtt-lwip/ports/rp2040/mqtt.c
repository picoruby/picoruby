/*
 * MQTT lwIP implementation for PicoRuby (RP2040)
 * Core MQTT logic using lwIP
 */

#include "../../include/mqtt.h"
#include "../../../picoruby-socket/include/socket.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "pico/stdlib.h"
#include <string.h>

static mqtt_context_t g_ctx;

static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
                               mqtt_connection_status_t status);
static void mqtt_incoming_publish_cb(void *arg, const char *topic,
                                     u32_t tot_len);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len,
                                  u8_t flags);
static void mqtt_request_cb(void *arg, err_t err);

static bool poll_state() {
  cyw43_arch_poll();

  switch (g_ctx.fsm_state) {
  case MQTT_STATE_ACTIVE:
    if (g_ctx.topic_to_sub[0] != '\0') {
      console_printf("[MQTT POLL] Processing subscription: %s\n",
                     g_ctx.topic_to_sub);
      g_ctx.fsm_state = MQTT_STATE_SUBSCRIBING;
      cyw43_arch_lwip_begin();
      mqtt_subscribe((mqtt_client_t*)g_ctx.client, g_ctx.topic_to_sub, 0, mqtt_request_cb,
                     &g_ctx);
      cyw43_arch_lwip_end();
      g_ctx.topic_to_sub[0] = '\0';
    } else if (g_ctx.topic_to_pub[0] != '\0') {
      console_printf("[MQTT POLL] Processing publish: %s\n",
                     g_ctx.topic_to_pub);
      g_ctx.fsm_state = MQTT_STATE_PUBLISHING;
      cyw43_arch_lwip_begin();
      mqtt_publish((mqtt_client_t*)g_ctx.client, g_ctx.topic_to_pub, g_ctx.payload_to_pub,
                   g_ctx.payload_to_pub_len, 0, 0, mqtt_request_cb, &g_ctx);
      cyw43_arch_lwip_end();
      g_ctx.topic_to_pub[0] = '\0';
    }
    break;
  case MQTT_STATE_ERROR:
    console_printf("[MQTT] State: ERROR\n");
    return false;
  case MQTT_STATE_DISCONNECTING:
    console_printf("[MQTT] State: DISCONNECTING\n");
    return false;
  default:
    break;
  }

  return true;
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
                               mqtt_connection_status_t status) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;
  LWIP_UNUSED_ARG(client);

  console_printf("[MQTT] Connection callback - status: %d\n", status);

  switch (status) {
  case MQTT_CONNECT_ACCEPTED:
    console_printf("[MQTT] Connected to broker!\n");
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  case MQTT_CONNECT_DISCONNECTED:
    console_printf("[MQTT] Disconnected from broker\n");
    ctx->fsm_state = MQTT_STATE_DISCONNECTING;
    break;
  default:
    console_printf("[MQTT] Connection failed: %d\n", status);
    ctx->fsm_state = MQTT_STATE_ERROR;
    break;
  }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic,
                                     u32_t tot_len) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;

  console_printf("[MQTT] Incoming publish topic: %s (len: %d)\n", topic,
                 tot_len);

  if (tot_len >= sizeof(ctx->recv_topic)) {
    console_printf("[MQTT ERROR] Topic too long\n");
    return;
  }

  strncpy(ctx->recv_topic, topic, sizeof(ctx->recv_topic) - 1);
  ctx->recv_topic[sizeof(ctx->recv_topic) - 1] = '\0';
  ctx->recv_payload_len = 0;
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len,
                                  u8_t flags) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;
  LWIP_UNUSED_ARG(flags);

  console_printf("[MQTT] Incoming data: %d bytes\n", len);

  if (ctx->recv_payload_len + len >= sizeof(ctx->recv_payload)) {
    console_printf("[MQTT ERROR] Payload too long\n");
    return;
  }

  memcpy(ctx->recv_payload + ctx->recv_payload_len, data, len);
  ctx->recv_payload_len += len;
  ctx->recv_payload[ctx->recv_payload_len] = '\0';

  if (flags & MQTT_DATA_FLAG_LAST) {
    ctx->message_arrived = true;
    console_printf("[MQTT] Complete message received: %s\n", ctx->recv_payload);
  }
}

static void mqtt_request_cb(void *arg, err_t err) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;

  console_printf("[MQTT] Request callback - err: %d\n", err);

  if (err != ERR_OK) {
    ctx->fsm_state = MQTT_STATE_ERROR;
    return;
  }

  switch (ctx->fsm_state) {
  case MQTT_STATE_CONNECTING:
    console_printf("[MQTT] Connection successful\n");
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  case MQTT_STATE_PUBLISHING:
    console_printf("[MQTT] Publish successful\n");
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  case MQTT_STATE_SUBSCRIBING:
    console_printf("[MQTT] Subscribe successful\n");
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  default:
    break;
  }
}

int MQTT_connect_impl(const char *host, int port, const char *client_id) {
  console_printf("[MQTT] Connecting to %s:%d with ID %s\n", host, port,
                 client_id);

  memset(&g_ctx, 0, sizeof(g_ctx));

  ip_addr_t ip;
  if (Net_get_ip(host, &ip) != 0) {
    console_printf("[MQTT] Failed to resolve host: %s\n", host);
    return -1;
  }

  g_ctx.client = (void*)mqtt_client_new();
  if (g_ctx.client == NULL) {
    console_printf("[MQTT] Failed to create client\n");
    return -1;
  }

  struct mqtt_connect_client_info_t client_info = {0};
  client_info.client_id = client_id;
  client_info.keep_alive = 60;

  mqtt_set_inpub_callback((mqtt_client_t*)g_ctx.client, mqtt_incoming_publish_cb,
                          mqtt_incoming_data_cb, &g_ctx);

  console_printf("[MQTT] Setting FSM state to CONNECTING\n");
  g_ctx.fsm_state = MQTT_STATE_CONNECTING;
  console_printf("[MQTT] Calling mqtt_client_connect (with lwIP locking)\n");
  cyw43_arch_lwip_begin();
  err_t err = mqtt_client_connect((mqtt_client_t*)g_ctx.client, &ip, port, mqtt_connection_cb,
                                  &g_ctx, &client_info);
  cyw43_arch_lwip_end();
  console_printf("[MQTT] mqtt_client_connect returned: %d (ERR_OK=%d)\n", err,
                 ERR_OK);

  if (err != ERR_OK) {
    console_printf("[MQTT] Connection failed: %d\n", err);
    mqtt_client_free((mqtt_client_t*)g_ctx.client);
    g_ctx.client = NULL;
    return -1;
  }

  // Wait for connection
  int timeout = 1000;
  while (g_ctx.fsm_state == MQTT_STATE_CONNECTING && timeout-- > 0) {
    if (!poll_state()) break;
    sleep_ms(10);
  }

  if (g_ctx.fsm_state != MQTT_STATE_ACTIVE) {
    console_printf("[MQTT] Connection timeout or failed\n");
    if (g_ctx.client) {
      mqtt_client_free((mqtt_client_t*)g_ctx.client);
      g_ctx.client = NULL;
    }
    return -1;
  }

  console_printf("[MQTT] Successfully connected!\n");
  return 0;
}

int MQTT_publish_impl(const char *topic, const char *payload, int len) {
  if (g_ctx.fsm_state != MQTT_STATE_ACTIVE) {
    console_printf("[MQTT] Not connected\n");
    return -1;
  }

  console_printf("[MQTT] Publishing to %s: %s\n", topic, payload);

  strncpy(g_ctx.topic_to_pub, topic, sizeof(g_ctx.topic_to_pub) - 1);
  g_ctx.topic_to_pub[sizeof(g_ctx.topic_to_pub) - 1] = '\0';

  strncpy(g_ctx.payload_to_pub, payload, sizeof(g_ctx.payload_to_pub) - 1);
  g_ctx.payload_to_pub[sizeof(g_ctx.payload_to_pub) - 1] = '\0';
  g_ctx.payload_to_pub_len = (len > 0) ? len : strlen(g_ctx.payload_to_pub);

  // Poll state will handle the actual publishing
  int timeout = 100;
  while (g_ctx.topic_to_pub[0] != '\0' && timeout-- > 0) {
    if (!poll_state()) return -1;
    sleep_ms(10);
  }

  return (g_ctx.topic_to_pub[0] == '\0') ? 0 : -1;
}

int MQTT_subscribe_impl(const char *topic) {
  if (g_ctx.fsm_state != MQTT_STATE_ACTIVE) {
    console_printf("[MQTT] Not connected\n");
    return -1;
  }

  console_printf("[MQTT] Subscribing to %s\n", topic);

  strncpy(g_ctx.topic_to_sub, topic, sizeof(g_ctx.topic_to_sub) - 1);
  g_ctx.topic_to_sub[sizeof(g_ctx.topic_to_sub) - 1] = '\0';

  // Poll state will handle the actual subscribing
  int timeout = 100;
  while (g_ctx.topic_to_sub[0] != '\0' && timeout-- > 0) {
    if (!poll_state()) return -1;
    sleep_ms(10);
  }

  return (g_ctx.topic_to_sub[0] == '\0') ? 0 : -1;
}

int MQTT_get_message_impl(char **topic, char **payload) {
  poll_state();

  if (!g_ctx.message_arrived) {
    return -1;
  }

  *topic = g_ctx.recv_topic;
  *payload = g_ctx.recv_payload;

  g_ctx.message_arrived = false;
  return 0;
}

void MQTT_disconnect_impl() {
  if (g_ctx.fsm_state != MQTT_STATE_IDLE) {
    cyw43_arch_lwip_begin();
    mqtt_disconnect((mqtt_client_t*)g_ctx.client);
    mqtt_client_free((mqtt_client_t*)g_ctx.client);
    cyw43_arch_lwip_end();
    memset(&g_ctx, 0, sizeof(g_ctx));
  }
}