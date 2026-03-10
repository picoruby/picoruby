/*
 * MQTT lwIP implementation for PicoRuby (RP2040)
 * Core MQTT logic using lwIP
 */

#include "../../include/mqtt.h"
#include "../../../picoruby-socket/include/socket.h"
#include "lwip/apps/mqtt.h"
#include "lwip/timeouts.h"
#include "lwip/stats.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <string.h>

static mqtt_context_t g_ctx;

// Debug helper function
static const char* mqtt_state_to_string(mqtt_fsm_state_t state) {
  switch (state) {
    case MQTT_STATE_IDLE: return "IDLE";
    case MQTT_STATE_CONNECTING: return "CONNECTING";
    case MQTT_STATE_CONNACK_WAIT: return "CONNACK_WAIT";
    case MQTT_STATE_ACTIVE: return "ACTIVE";
    case MQTT_STATE_SUBSCRIBING: return "SUBSCRIBING";
    case MQTT_STATE_PUBLISHING: return "PUBLISHING";
    case MQTT_STATE_DISCONNECTING: return "DISCONNECTING";
    case MQTT_STATE_ERROR: return "ERROR";
    case MQTT_STATE_TIMEOUT: return "TIMEOUT";
    default: return "UNKNOWN";
  }
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg,
                               mqtt_connection_status_t status);
static void mqtt_incoming_publish_cb(void *arg, const char *topic,
                                     u32_t tot_len);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len,
                                  u8_t flags);
static void mqtt_request_cb(void *arg, err_t err);

static bool poll_state() {
  // Multiple polling to ensure network events are processed
  for (int i = 0; i < 10; i++) {
    cyw43_arch_poll();
  }

  static int poll_count = 0;
  poll_count++;

  // Only print every 100 calls to avoid spam
  if (poll_count % 100 == 0) {
    console_printf("[MQTT POLL] poll_state() called %d times, current state=%s(%d)\n",
                   poll_count, mqtt_state_to_string(g_ctx.fsm_state), g_ctx.fsm_state);
  }

  switch (g_ctx.fsm_state) {
  case MQTT_STATE_ACTIVE:
    if (g_ctx.topic_to_sub[0] != '\0') {
      console_printf("[MQTT POLL] Processing subscription\n");
      g_ctx.fsm_state = MQTT_STATE_SUBSCRIBING;
      lwip_begin();
      mqtt_subscribe((mqtt_client_t*)g_ctx.client, g_ctx.topic_to_sub, 0, mqtt_request_cb,
                     &g_ctx);
      lwip_end();
      g_ctx.topic_to_sub[0] = '\0';
    } else if (g_ctx.topic_to_pub[0] != '\0') {
      console_printf("[MQTT POLL] Processing publish\n");
      g_ctx.fsm_state = MQTT_STATE_PUBLISHING;
      lwip_begin();
      mqtt_publish((mqtt_client_t*)g_ctx.client, g_ctx.topic_to_pub, g_ctx.payload_to_pub,
                   g_ctx.payload_to_pub_len, 0, 0, mqtt_request_cb, &g_ctx);
      lwip_end();
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

  console_printf("[DEBUG] mqtt_connection_cb CALLED! status=%d\n", status);
  console_printf("[MQTT CALLBACK] Connection callback called! client=%p, arg=%p, status=%d\n",
                 (void*)client, arg, status);

  if (ctx == NULL) {
    console_printf("[MQTT CALLBACK ERROR] ctx is NULL!\n");
    return;
  }

  console_printf("[MQTT CALLBACK] Previous fsm_state=%s(%d)\n",
                 mqtt_state_to_string(ctx->fsm_state), ctx->fsm_state);

  switch (status) {
  case MQTT_CONNECT_ACCEPTED:
    console_printf("[MQTT CALLBACK] MQTT_CONNECT_ACCEPTED - Connected to broker!\n");
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    break;
  case MQTT_CONNECT_DISCONNECTED:
    console_printf("[MQTT CALLBACK] MQTT_CONNECT_DISCONNECTED - Disconnected from broker\n");
    ctx->fsm_state = MQTT_STATE_DISCONNECTING;
    break;
  default:
    console_printf("[MQTT CALLBACK] Unknown status %d - Connection failed\n", status);
    ctx->fsm_state = MQTT_STATE_ERROR;
    break;
  }

  console_printf("[MQTT CALLBACK] New fsm_state=%s(%d)\n",
                 mqtt_state_to_string(ctx->fsm_state), ctx->fsm_state);
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic,
                                     u32_t tot_len) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;

  console_printf("[MQTT] Incoming publish topic\n");

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

  console_printf("[MQTT] Incoming data\n");

  if (ctx->recv_payload_len + len >= sizeof(ctx->recv_payload)) {
    console_printf("[MQTT ERROR] Payload too long\n");
    return;
  }

  memcpy(ctx->recv_payload + ctx->recv_payload_len, data, len);
  ctx->recv_payload_len += len;
  ctx->recv_payload[ctx->recv_payload_len] = '\0';

  if (flags & MQTT_DATA_FLAG_LAST) {
    ctx->message_arrived = true;
    console_printf("[MQTT] Complete message received\n");
  }
}

static void mqtt_request_cb(void *arg, err_t err) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;

  console_printf("[DEBUG] mqtt_request_cb called: err=%d\n", err);

  if (err != ERR_OK) {
    console_printf("[DEBUG] Request failed with error: %d\n", err);
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
  console_printf("[MQTT] Starting non-blocking connect to %s:%d\n", host, port);

  memset(&g_ctx, 0, sizeof(g_ctx));

  ip_addr_t ip;
  if (Net_get_ip(host, &ip) != 0) {
    console_printf("[MQTT] Failed to resolve host\n");
    return -1;
  }

  console_printf("[MQTT] DNS resolved successfully to: %d.%d.%d.%d\n",
                 (int)(ip.addr & 0xFF),
                 (int)((ip.addr >> 8) & 0xFF),
                 (int)((ip.addr >> 16) & 0xFF),
                 (int)((ip.addr >> 24) & 0xFF));

  lwip_begin();
  g_ctx.client = (void*)mqtt_client_new();
  lwip_end();

  if (g_ctx.client == NULL) {
    console_printf("[MQTT] Failed to create client\n");
    return -1;
  }

  struct mqtt_connect_client_info_t client_info = {0};
  client_info.client_id = client_id;
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
    console_printf("[MQTT] mqtt_client_connect failed with error %d\n", err);
    lwip_begin();
    mqtt_client_free((mqtt_client_t*)g_ctx.client);
    lwip_end();
    g_ctx.client = NULL;
    return -1;
  }

  console_printf("[MQTT] Connection initiated successfully (non-blocking)\n");
  return 0; // Return immediately, connection will complete asynchronously
}

int MQTT_publish_impl(const char *topic, const char *payload, int len) {
  if (g_ctx.fsm_state != MQTT_STATE_ACTIVE) {
    console_printf("[MQTT] Not connected\n");
    return -1;
  }

  console_printf("[MQTT] Publishing message\n");

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
    console_printf("[MQTT] Not connected\n");
    return -1;
  }

  console_printf("[MQTT] Subscribing to topic\n");

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
  console_printf("[C] MQTT_is_connected_impl called\n");

  static int check_count = 0;

  // Process network events to advance TCP connection
  lwip_begin();
  cyw43_arch_poll();      // Process async context
  sys_check_timeouts();   // Process lwIP timers
  lwip_end();

  // Update MQTT state
  poll_state();

  // Debug log every call (since we're testing)
  check_count++;
  console_printf("[C] Connection check #%d: state=%s\n",
                 check_count, mqtt_state_to_string(g_ctx.fsm_state));

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