#include "../include/mqtt.h"
#include "../../picoruby-net/include/net.h"

#ifdef PICO_BUILD
#include "pico/cyw43_arch.h"
#endif

static mqtt_context_t g_ctx;

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqtt_request_cb(void *arg, err_t err);

static bool poll_state() {
#ifdef PICO_BUILD
  cyw43_arch_poll();
#endif

  switch (g_ctx.fsm_state) {
    case MQTT_STATE_ACTIVE:
      if (g_ctx.topic_to_sub[0] != '\0') {
        console_printf("[MQTT POLL] Processing subscription: %s\n", g_ctx.topic_to_sub);
        g_ctx.fsm_state = MQTT_STATE_SUBSCRIBING;
#ifdef PICO_BUILD
        cyw43_arch_lwip_begin();
#endif
        mqtt_subscribe(g_ctx.client, g_ctx.topic_to_sub, 0, mqtt_request_cb, &g_ctx);
#ifdef PICO_BUILD
        cyw43_arch_lwip_end();
#endif
        g_ctx.topic_to_sub[0] = '\0';
      } else if (g_ctx.topic_to_pub[0] != '\0') {
        console_printf("[MQTT POLL] Processing publish: %s\n", g_ctx.topic_to_pub);
        g_ctx.fsm_state = MQTT_STATE_PUBLISHING;
#ifdef PICO_BUILD
        cyw43_arch_lwip_begin();
#endif
        mqtt_publish(g_ctx.client, g_ctx.topic_to_pub, g_ctx.payload_to_pub, g_ctx.payload_to_pub_len, 0, 0, mqtt_request_cb, &g_ctx);
#ifdef PICO_BUILD
        cyw43_arch_lwip_end();
#endif
        g_ctx.topic_to_pub[0] = '\0';
      }
      break;
    case MQTT_STATE_ERROR:
    case MQTT_STATE_TIMEOUT:
      console_printf("[MQTT POLL] State is ERROR or TIMEOUT, returning false\n");
      return false;
    default:
      if (g_ctx.fsm_state == MQTT_STATE_CONNECTING) {
        static int poll_count = 0;
        poll_count++;
        if (poll_count % 100 == 0) {
          console_printf("[MQTT POLL] Still in CONNECTING state, poll count: %d\n", poll_count);
        }
      }
      break;
  }
  return true;
}

int MQTT_connect_impl(const char *host, int port, const char *client_id) {
  console_printf("[MQTT] MQTT_connect_impl called: host=%s, port=%d, client_id=%s\n", host, port, client_id);
  console_printf("[MQTT] Current FSM state: %d (IDLE=%d)\n", g_ctx.fsm_state, MQTT_STATE_IDLE);
  
  if (g_ctx.fsm_state != MQTT_STATE_IDLE) {
    console_printf("[MQTT ERROR] Not in IDLE state, current state: %d\n", g_ctx.fsm_state);
    return -1;
  }

  console_printf("[MQTT] Resolving IP for host: %s\n", host);
  ip_addr_t ip;
  err_t dns_err = Net_get_ip(host, &ip);
  if (dns_err != ERR_OK) {
    console_printf("[MQTT ERROR] DNS resolution failed: err=%d\n", dns_err);
    return -1;
  }
  console_printf("[MQTT] DNS resolved successfully\n");

  console_printf("[MQTT] Creating MQTT client\n");
  g_ctx.client = mqtt_client_new();
  if (!g_ctx.client) {
    console_printf("[MQTT ERROR] Failed to create MQTT client\n");
    return -1;
  }
  console_printf("[MQTT] MQTT client created: %p\n", g_ctx.client);

  struct mqtt_connect_client_info_t client_info = {
    .client_id = client_id,
    .keep_alive = 60,
  };

  console_printf("[MQTT] Setting FSM state to CONNECTING\n");
  g_ctx.fsm_state = MQTT_STATE_CONNECTING;
  console_printf("[MQTT] Calling mqtt_client_connect (with lwIP locking)\n");
#ifdef PICO_BUILD
  cyw43_arch_lwip_begin();
#endif
  err_t err = mqtt_client_connect(g_ctx.client, &ip, port, mqtt_connection_cb, &g_ctx, &client_info);
#ifdef PICO_BUILD
  cyw43_arch_lwip_end();
#endif
  console_printf("[MQTT] mqtt_client_connect returned: %d (ERR_OK=%d)\n", err, ERR_OK);

  if (err != ERR_OK) {
    console_printf("[MQTT ERROR] mqtt_client_connect failed: %d\n", err);
    g_ctx.fsm_state = MQTT_STATE_ERROR;
    return -1;
  }

  console_printf("[MQTT] Starting connection wait loop (10 seconds timeout)\n");
  console_printf("[MQTT] About to enter while loop: state=%d (ACTIVE=%d, ERROR=%d), timeout=%d\n", 
                 g_ctx.fsm_state, MQTT_STATE_ACTIVE, MQTT_STATE_ERROR, 10000);
  int timeout_ms = 10000;
  int elapsed = 0;
  while (g_ctx.fsm_state != MQTT_STATE_ACTIVE && g_ctx.fsm_state != MQTT_STATE_ERROR && timeout_ms > 0) {
#ifdef PICO_BUILD
    cyw43_arch_poll();
#endif
    poll_state();
    Net_sleep_ms(10);
    timeout_ms -= 10;
    elapsed += 10;

    if (elapsed % 1000 == 0) {
      console_printf("[MQTT] Connection attempt... %d/%d seconds, state=%d\n", elapsed/1000, 10, g_ctx.fsm_state);
    }
  }

  console_printf("[MQTT] Connection wait finished. Final state: %d (ACTIVE=%d, ERROR=%d)\n", 
                 g_ctx.fsm_state, MQTT_STATE_ACTIVE, MQTT_STATE_ERROR);

  return (g_ctx.fsm_state == MQTT_STATE_ACTIVE) ? 0 : -1;
}

int MQTT_subscribe_impl(const char *topic) {
  if (g_ctx.fsm_state != MQTT_STATE_ACTIVE) return -1;
  strncpy(g_ctx.topic_to_sub, topic, MQTT_TOPIC_MAX_LEN - 1);
  
  int timeout_ms = 5000;
  while (g_ctx.fsm_state != MQTT_STATE_SUBSCRIBING && timeout_ms > 0) {
    poll_state();
    Net_sleep_ms(10);
    timeout_ms -= 10;
  }
  // Wait for SUBACK
  while (g_ctx.fsm_state == MQTT_STATE_SUBSCRIBING && timeout_ms > 0) {
    poll_state();
    Net_sleep_ms(10);
    timeout_ms -= 10;
  }
  return (g_ctx.fsm_state == MQTT_STATE_ACTIVE) ? 0 : -1;
}

int MQTT_publish_impl(const char *topic, const char *payload, int len) {
  if (g_ctx.fsm_state != MQTT_STATE_ACTIVE) return -1;
  strncpy(g_ctx.topic_to_pub, topic, MQTT_TOPIC_MAX_LEN - 1);
  strncpy(g_ctx.payload_to_pub, payload, MQTT_PAYLOAD_MAX_LEN - 1);
  g_ctx.payload_to_pub_len = len;

  int timeout_ms = 5000;
  while (g_ctx.fsm_state != MQTT_STATE_PUBLISHING && timeout_ms > 0) {
    poll_state();
    Net_sleep_ms(10);
    timeout_ms -= 10;
  }
  while (g_ctx.fsm_state == MQTT_STATE_PUBLISHING && timeout_ms > 0) {
    poll_state();
    Net_sleep_ms(10);
    timeout_ms -= 10;
  }
  return (g_ctx.fsm_state == MQTT_STATE_ACTIVE) ? 0 : -1;
}

void MQTT_disconnect_impl() {
  if (g_ctx.fsm_state != MQTT_STATE_IDLE) {
#ifdef PICO_BUILD
    cyw43_arch_lwip_begin();
#endif
    mqtt_disconnect(g_ctx.client);
    mqtt_client_free(g_ctx.client);
#ifdef PICO_BUILD
    cyw43_arch_lwip_end();
#endif
    memset(&g_ctx, 0, sizeof(g_ctx));
  }
}

int MQTT_get_message_impl(char **topic, char **payload) {
  poll_state();
  if (g_ctx.message_arrived) {
    *topic = g_ctx.recv_topic;
    *payload = g_ctx.recv_payload;
    int len = g_ctx.recv_payload_len;
    g_ctx.message_arrived = false;
    return len;
  }
  return -1;
}

// lwIP callback implementations
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;

  if (status == MQTT_CONNECT_ACCEPTED) {
    ctx->fsm_state = MQTT_STATE_ACTIVE;
    mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, ctx);
  } else {
    ctx->fsm_state = MQTT_STATE_ERROR;
  }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;
  strncpy(ctx->recv_topic, topic, MQTT_TOPIC_MAX_LEN - 1);
  ctx->recv_payload_len = 0;
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;
  if (ctx->recv_payload_len + len < MQTT_PAYLOAD_MAX_LEN) {
    memcpy(ctx->recv_payload + ctx->recv_payload_len, data, len);
    ctx->recv_payload_len += len;
  }
  if (flags & MQTT_DATA_FLAG_LAST) {
    ctx->recv_payload[ctx->recv_payload_len] = '\0';
    ctx->message_arrived = true;
  }
}

static void mqtt_request_cb(void *arg, err_t err) {
  mqtt_context_t *ctx = (mqtt_context_t *)arg;
  if (err == ERR_OK) {
    if (ctx->fsm_state == MQTT_STATE_SUBSCRIBING || ctx->fsm_state == MQTT_STATE_PUBLISHING) {
      ctx->fsm_state = MQTT_STATE_ACTIVE;
    }
  } else {
    ctx->fsm_state = MQTT_STATE_ERROR;
  }
}

void MQTT_init_context(void *vm) {
  memset(&g_ctx, 0, sizeof(g_ctx));
#if defined(PICORB_VM_MRUBY)
  g_ctx.mrb = (mrb_state *)vm;
  g_ctx.callback_proc = mrb_nil_value();
#elif defined(PICORB_VM_MRUBYC)
  g_ctx.vm = (mrbc_vm *)vm;
  g_ctx.callback_proc = mrbc_nil_value();
#endif
}

#if defined(PICORB_VM_MRUBY)
#include "mruby/mqtt.c"
#elif defined(PICORB_VM_MRUBYC)
#include "mrubyc/mqtt.c"
#endif
