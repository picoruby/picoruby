/*
 * MQTT lwIP implementation for PicoRuby (RP2040)
 * Core MQTT logic using lwIP
 */

#include "../../include/mqtt.h"
#include "../../../picoruby-socket/include/socket.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/timeouts.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/ip_addr.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <string.h>

static mqtt_context_t g_ctx;
static int g_poll_count;

static u16_t mqtt_ringbuf_len(struct mqtt_ringbuf_t *rb) {
  u32_t len = rb->put - rb->get;
  if (len > 0xFFFF) {
    len += MQTT_OUTPUT_RINGBUF_SIZE;
  }
  return (u16_t)len;
}

static u16_t mqtt_ringbuf_linear_len(struct mqtt_ringbuf_t *rb) {
  u16_t len = mqtt_ringbuf_len(rb);
  u16_t linear = MQTT_OUTPUT_RINGBUF_SIZE - rb->get;
  return (len < linear) ? len : linear;
}

static void mqtt_force_send(mqtt_client_t *client) {
  if (client == NULL || client->conn == NULL) {
    return;
  }

  u16_t ring_len = mqtt_ringbuf_len(&client->output);
  if (ring_len == 0) {
    return;
  }

  u16_t send_len = mqtt_ringbuf_linear_len(&client->output);
  u16_t sndbuf = altcp_sndbuf(client->conn);
  if (sndbuf == 0 || send_len == 0) {
    return;
  }
  if (send_len > sndbuf) {
    send_len = sndbuf;
  }

  err_t err = altcp_write(client->conn, &client->output.buf[client->output.get],
                          send_len, TCP_WRITE_FLAG_COPY);
  if (err == ERR_OK) {
    client->output.get += send_len;
    if (client->output.get >= MQTT_OUTPUT_RINGBUF_SIZE) {
      client->output.get -= MQTT_OUTPUT_RINGBUF_SIZE;
    }
    altcp_output(client->conn);
  } else {
    console_printf("[MQTT] force_send error %d\n", err);
  }
}

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

static void test_tcp_raw_connect() {
  console_printf("\n[TCP_TEST] === Starting Enhanced Raw TCP Test ===\n");

  ip_addr_t test_ip;
  IP4_ADDR(&test_ip, 192, 168, 0, 200);

  lwip_begin();

  // ネットワークインターフェース状況確認
  extern struct netif *netif_default;
  if (netif_default) {
    console_printf("[TCP_TEST] Pre-connect netif: ip=%s, up=%d, link_up=%d\n",
                   ip4addr_ntoa(&netif_default->ip_addr),
                   netif_is_up(netif_default),
                   netif_is_link_up(netif_default));
  } else {
    console_printf("[TCP_TEST] ERROR: No default netif!\n");
    lwip_end();
    return;
  }

  // TCPプロトコルコントロールブロック作成
  struct tcp_pcb *pcb = tcp_new();
  if (!pcb) {
    console_printf("[TCP_TEST] ERROR: tcp_new() failed\n");
    lwip_end();
    return;
  }

  console_printf("[TCP_TEST] PCB created: %p\n", pcb);
  console_printf("[TCP_TEST] PCB state: %d (CLOSED=%d)\n", pcb->state, CLOSED);

  // 接続試行
  console_printf("[TCP_TEST] Calling tcp_connect to 192.168.0.200:1883\n");
  err_t err = tcp_connect(pcb, &test_ip, 1883, NULL);
  console_printf("[TCP_TEST] tcp_connect returned: %d (ERR_OK=%d)\n", err, ERR_OK);

  if (err == ERR_OK) {
    console_printf("[TCP_TEST] tcp_connect succeeded, state=%d\n", pcb->state);

    // 積極的なネットワーク処理（10回のラウンド）
    for (int round = 0; round < 10; round++) {
      console_printf("[TCP_TEST] === Round %d ===\n", round);

      // CYW43ポーリング
      lwip_end();
      for (int j = 0; j < 5; j++) {
        cyw43_arch_poll();
      }
      lwip_begin();

      // タイマー処理
      console_printf("[TCP_TEST] Processing timers...\n");
      sys_check_timeouts();

      // TCP状態確認
      console_printf("[TCP_TEST] PCB state=%d (SYN_SENT=%d, ESTABLISHED=%d)\n",
                     pcb->state, SYN_SENT, ESTABLISHED);

      // 強制的なTCP出力（複数回試行）
      console_printf("[TCP_TEST] Forcing tcp_output() x3\n");
      tcp_output(pcb);
      tcp_output(pcb);
      tcp_output(pcb);

      // 状態が変化した場合は終了
      if (pcb->state == ESTABLISHED) {
        console_printf("[TCP_TEST] Connection ESTABLISHED!\n");
        break;
      } else if (pcb->state != SYN_SENT && pcb->state != CLOSED) {
        console_printf("[TCP_TEST] Unexpected state: %d\n", pcb->state);
        break;
      }

      // 短い待機
      lwip_end();
      Net_busy_wait_ms(100);
      lwip_begin();
    }

    console_printf("[TCP_TEST] Final state=%d\n", pcb->state);
  }

  // クリーンアップ
  tcp_abort(pcb);
  lwip_end();

  console_printf("[TCP_TEST] === Enhanced Test Complete ===\n\n");
}

void MQTT_poll_impl(void) {
  for (int i = 0; i < 3; i++) {
    cyw43_arch_poll();
  }
  lwip_begin();
  sys_check_timeouts();
  mqtt_client_t *client = (mqtt_client_t *)g_ctx.client;
  if (client != NULL) {
    mqtt_force_send(client);
    u16_t ring_len = mqtt_ringbuf_len(&client->output);
    u16_t sndbuf = client->conn ? altcp_sndbuf(client->conn) : 0;
    if (g_poll_count < 200 || (g_poll_count % 50) == 0) {
      console_printf("[MQTT] poll: state=%d ringbuf=%u sndbuf=%u put=%u get=%u size=%u\n",
                     client->conn_state, ring_len, sndbuf,
                     client->output.put, client->output.get, MQTT_OUTPUT_RINGBUF_SIZE);
    }
    if (client->conn_state == 2 && ring_len == 0) {
      console_printf("[MQTT] WARN: MQTT_CONNECTING with empty ringbuf\n");
    }
    g_poll_count++;
  }
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

  console_printf("\n[MEMORY] In mqtt_connection_cb:\n");
  stats_display();
  console_printf("[CALLBACK] mqtt_connection_cb ENTERED! status=%d\n", status);
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

  console_printf("\n[MEMORY] Initial memory state:\n");
  stats_display();

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

  console_printf("\n[MEMORY] After DNS resolution:\n");
  stats_display();

  lwip_begin();
  g_ctx.client = (void*)mqtt_client_new();
  lwip_end();

  console_printf("\n[MEMORY] After mqtt_client_new():\n");
  if (g_ctx.client == NULL) {
    console_printf("[MQTT] ERROR: mqtt_client_new() returned NULL - OUT OF MEMORY?\n");
    stats_display();
    return -1;
  }
  console_printf("[MQTT] mqtt_client_new() succeeded, client=%p\n", g_ctx.client);
  stats_display();

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

  console_printf("\n[MEMORY] Before mqtt_client_connect():\n");
  stats_display();

  lwip_begin();
  err_t err = mqtt_client_connect((mqtt_client_t*)g_ctx.client, &ip, port, mqtt_connection_cb,
                                  &g_ctx, &client_info);
  lwip_end();

  console_printf("\n[MEMORY] After mqtt_client_connect(), err=%d:\n", err);
  stats_display();

  if (err != ERR_OK) {
    console_printf("[MQTT] mqtt_client_connect failed with error %d\n", err);
    lwip_begin();
    mqtt_client_free((mqtt_client_t*)g_ctx.client);
    lwip_end();
    g_ctx.client = NULL;
    return -1;
  }

  lwip_begin();
  mqtt_client_t *client = (mqtt_client_t *)g_ctx.client;
  console_printf("[MQTT] after connect: state=%d put=%u get=%u ringbuf=%u\n",
                 client->conn_state, client->output.put, client->output.get,
                 mqtt_ringbuf_len(&client->output));
  lwip_end();

  return 0;
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
  MQTT_poll_impl();

  // Check TCP connection status directly
  lwip_begin();
  u8_t tcp_connected = mqtt_client_is_connected((mqtt_client_t*)g_ctx.client);
  lwip_end();

  // Update MQTT state
  poll_state();

  console_printf("[C] is_connected: tcp_connected=%d, state=%s\n",
                 tcp_connected, mqtt_state_to_string(g_ctx.fsm_state));

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
