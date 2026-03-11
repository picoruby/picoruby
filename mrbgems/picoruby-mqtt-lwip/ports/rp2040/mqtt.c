/*
 * MQTT lwIP implementation for PicoRuby (RP2040)
 * Core MQTT logic using lwIP
 */

#include "../../include/mqtt.h"
#include "../../../picoruby-socket/include/socket.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/timeouts.h"
#include "lwip/ip_addr.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <string.h>

static mqtt_context_t g_ctx;
static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
                               mqtt_connection_status_t status);
static void mqtt_incoming_publish_cb(void *arg, const char *topic,
                                     u32_t tot_len);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len,
                                  u8_t flags);
static void mqtt_request_cb(void *arg, err_t err);

void MQTT_poll_impl(void) {
  for (int i = 0; i < 3; i++) {
    cyw43_arch_poll();
  }
  lwip_begin();
  sys_check_timeouts();
  lwip_end();
}

void MQTT_poll_sleep_impl(int ms) {
  for (int i = 0; i < ms; i++) {
    MQTT_poll_impl();
    sleep_ms(1);
  }
}

static bool poll_state() {
  MQTT_poll_impl();
  // Multiple polling to ensure network events are processed OUTSIDE lwIP lock
  for (int i = 0; i < 10; i++) {
    cyw43_arch_poll();
  }

  switch (g_ctx.fsm_state) {
  case MQTT_STATE_ACTIVE:
    if (g_ctx.topic_to_sub[0] != '\0') {
      g_ctx.fsm_state = MQTT_STATE_SUBSCRIBING;
      lwip_begin();
      mqtt_subscribe((mqtt_client_t*)g_ctx.client, g_ctx.topic_to_sub, 0, mqtt_request_cb,
                     &g_ctx);
      lwip_end();
      g_ctx.topic_to_sub[0] = '\0';
    } else if (g_ctx.topic_to_pub[0] != '\0') {
      g_ctx.fsm_state = MQTT_STATE_PUBLISHING;
      lwip_begin();
      mqtt_publish((mqtt_client_t*)g_ctx.client, g_ctx.topic_to_pub, g_ctx.payload_to_pub,
                   g_ctx.payload_to_pub_len, 0, 0, mqtt_request_cb, &g_ctx);
      lwip_end();
      g_ctx.topic_to_pub[0] = '\0';
    }
    break;
  case MQTT_STATE_ERROR:
    return false;
  case MQTT_STATE_DISCONNECTING:
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

  if (ctx == NULL) {
    return;
  }

  switch (status) {
  case MQTT_CONNECT_ACCEPTED:
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  case MQTT_CONNECT_DISCONNECTED:
    ctx->fsm_state = MQTT_STATE_DISCONNECTING;
    break;
  default:
    ctx->fsm_state = MQTT_STATE_ERROR;
    break;
  }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic,
                                     u32_t tot_len) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;

  if (tot_len >= sizeof(ctx->recv_topic)) {
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

  if (ctx->recv_payload_len + len >= sizeof(ctx->recv_payload)) {
    return;
  }

  memcpy(ctx->recv_payload + ctx->recv_payload_len, data, len);
  ctx->recv_payload_len += len;
  ctx->recv_payload[ctx->recv_payload_len] = '\0';

  if (flags & MQTT_DATA_FLAG_LAST) {
    ctx->message_arrived = true;
  }
}

static void mqtt_request_cb(void *arg, err_t err) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;

  if (err != ERR_OK) {
    ctx->fsm_state = MQTT_STATE_ERROR;
    return;
  }

  switch (ctx->fsm_state) {
  case MQTT_STATE_CONNECTING:
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  case MQTT_STATE_PUBLISHING:
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  case MQTT_STATE_SUBSCRIBING:
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  default:
    break;
  }
}

int MQTT_connect_impl(const char *host, int port, const char *client_id) {
  memset(&g_ctx, 0, sizeof(g_ctx));

  ip_addr_t ip;
  if (Net_get_ip(host, &ip) != 0) {
    return -1;
  }

  lwip_begin();
  g_ctx.client = (void*)mqtt_client_new();
  lwip_end();

  if (g_ctx.client == NULL) {
    return -1;
  }

  struct mqtt_connect_client_info_t client_info = {0};
  strncpy(g_ctx.client_id, client_id, sizeof(g_ctx.client_id) - 1);
  g_ctx.client_id[sizeof(g_ctx.client_id) - 1] = '\0';
  client_info.client_id = g_ctx.client_id;
  client_info.keep_alive = 60;

  lwip_begin();
  mqtt_set_inpub_callback((mqtt_client_t*)g_ctx.client, mqtt_incoming_publish_cb,
                          mqtt_incoming_data_cb, &g_ctx);
  lwip_end();

  g_ctx.fsm_state = MQTT_STATE_CONNECTING;

  lwip_begin();
  err_t err = mqtt_client_connect((mqtt_client_t*)g_ctx.client, &ip, port, mqtt_connection_cb,
                                  &g_ctx, &client_info);
  lwip_end();

  if (err != ERR_OK) {
    lwip_begin();
    mqtt_client_free((mqtt_client_t*)g_ctx.client);
    lwip_end();
    g_ctx.client = NULL;
    return -1;
  }

  return 0;
}

int MQTT_publish_impl(const char *topic, const char *payload, int len) {
  if (g_ctx.fsm_state != MQTT_STATE_ACTIVE) {
    return -1;
  }

  strncpy(g_ctx.topic_to_pub, topic, sizeof(g_ctx.topic_to_pub) - 1);
  g_ctx.topic_to_pub[sizeof(g_ctx.topic_to_pub) - 1] = '\0';

  strncpy(g_ctx.payload_to_pub, payload, sizeof(g_ctx.payload_to_pub) - 1);
  g_ctx.payload_to_pub[sizeof(g_ctx.payload_to_pub) - 1] = '\0';
  g_ctx.payload_to_pub_len = (len > 0) ? len : strlen(g_ctx.payload_to_pub);

  // Poll state will handle the actual publishing
  int timeout = 100;
  while (g_ctx.topic_to_pub[0] != '\0' && timeout-- > 0) {
    if (!poll_state()) return -1;
    Net_busy_wait_ms(10);
  }

  return (g_ctx.topic_to_pub[0] == '\0') ? 0 : -1;
}

int MQTT_subscribe_impl(const char *topic) {
  if (g_ctx.fsm_state != MQTT_STATE_ACTIVE) {
    return -1;
  }

  strncpy(g_ctx.topic_to_sub, topic, sizeof(g_ctx.topic_to_sub) - 1);
  g_ctx.topic_to_sub[sizeof(g_ctx.topic_to_sub) - 1] = '\0';

  // Poll state will handle the actual subscribing
  int timeout = 100;
  while (g_ctx.topic_to_sub[0] != '\0' && timeout-- > 0) {
    if (!poll_state()) return -1;
    Net_busy_wait_ms(10);
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

int MQTT_is_connected_impl() {
  MQTT_poll_impl();

  // Check TCP connection status directly
  lwip_begin();
  u8_t tcp_connected = mqtt_client_is_connected((mqtt_client_t*)g_ctx.client);
  lwip_end();

  // Update MQTT state
  poll_state();

  return (g_ctx.fsm_state == MQTT_STATE_ACTIVE) ? 1 : 0;
}

void MQTT_disconnect_impl() {
  if (g_ctx.fsm_state != MQTT_STATE_IDLE) {
    lwip_begin();
    mqtt_disconnect((mqtt_client_t*)g_ctx.client);
    mqtt_client_free((mqtt_client_t*)g_ctx.client);
    lwip_end();
    memset(&g_ctx, 0, sizeof(g_ctx));
  }
}
