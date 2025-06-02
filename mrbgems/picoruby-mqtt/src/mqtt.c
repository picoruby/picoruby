#include "../include/mqtt.h"
#include "lwip/tcp.h"
#ifdef PICORB_VM_MRUBY
#include "mruby.h"
#else
#include "mrubyc.h"
#endif

static mqtt_client_t *g_mqtt_client = NULL;

void
MQTT_push_event(uint8_t *data, uint16_t size)
{
  if (!g_mqtt_client || g_mqtt_client->packet_mutex) return;
  g_mqtt_client->packet_mutex = true;

  if (g_mqtt_client->packet_buffer != NULL) {
    picorb_free(g_mqtt_client->vm, g_mqtt_client->packet_buffer);
  }

  g_mqtt_client->packet_buffer = picorb_alloc(g_mqtt_client->vm, size);
  memcpy(g_mqtt_client->packet_buffer, data, size);
  g_mqtt_client->packet_size = size;
  g_mqtt_client->packet_available = true;
  g_mqtt_client->in_callback = true;  // Mark that we're in callback context

  g_mqtt_client->packet_mutex = false;
}

bool
MQTT_pop_event(uint8_t **data, uint16_t *size)
{
  if (!g_mqtt_client || g_mqtt_client->packet_mutex || !g_mqtt_client->packet_available) return false;
  g_mqtt_client->packet_mutex = true;

  *data = g_mqtt_client->packet_buffer;
  *size = g_mqtt_client->packet_size;

  g_mqtt_client->packet_buffer = NULL;
  g_mqtt_client->packet_available = false;
  g_mqtt_client->in_callback = false;  // Clear callback context flag

  g_mqtt_client->packet_mutex = false;
  return true;
}

static uint8_t*
create_connect_packet(const char *client_id, size_t *packet_len)
{
  const char *protocol_name = "MQTT";
  uint8_t protocol_level = 4; // MQTT 3.1.1
  uint8_t connect_flags = 0x02; // Clean Session
  uint16_t keep_alive = 60; // 60 seconds

  size_t client_id_len = strlen(client_id);
  size_t protocol_name_len = strlen(protocol_name);

  size_t remaining_len = 2 + protocol_name_len + // Protocol name length + name
                        1 + // Protocol level
                        1 + // Connect flags
                        2 + // Keep alive
                        2 + client_id_len; // Client ID length + ID

  // Allocate buffer for variable length encoding (max 4 bytes)
  uint8_t var_len_buf[4];
  size_t var_len_size = encode_variable_length(remaining_len, var_len_buf);

  // Total packet length
  *packet_len = 1 + var_len_size + remaining_len;

  uint8_t *packet = (uint8_t *)picorb_alloc(g_mqtt_client->vm, *packet_len);
  if (!packet) return NULL;

  size_t pos = 0;

  packet[pos++] = MQTT_CONNECT << 4;

  memcpy(packet + pos, var_len_buf, var_len_size);
  pos += var_len_size;

  packet[pos++] = 0;
  packet[pos++] = protocol_name_len;
  memcpy(packet + pos, protocol_name, protocol_name_len);
  pos += protocol_name_len;

  packet[pos++] = protocol_level;

  packet[pos++] = connect_flags;

  packet[pos++] = keep_alive >> 8;
  packet[pos++] = keep_alive & 0xFF;

  packet[pos++] = client_id_len >> 8;
  packet[pos++] = client_id_len & 0xFF;
  memcpy(packet + pos, client_id, client_id_len);

  return packet;
}

size_t
encode_variable_length(size_t length, uint8_t *buf)
{
  size_t pos = 0;
  do {
    uint8_t byte = length % 128;
    length = length / 128;
    if (length > 0) {
      byte |= 0x80;
    }
    buf[pos++] = byte;
  } while (length > 0);
  return pos;
}

static uint8_t*
create_publish_packet(const char *topic, const char *payload, size_t *packet_len)
{
  size_t topic_len = strlen(topic);
  size_t payload_len = strlen(payload);
  uint16_t packet_id = 1;

  size_t remaining_len = 2 + topic_len + 2 + payload_len;

  uint8_t var_len_buf[4];
  size_t var_len_size = encode_variable_length(remaining_len, var_len_buf);

  *packet_len = 1 + var_len_size + remaining_len;

  uint8_t *packet = (uint8_t *)picorb_alloc(g_mqtt_client->vm, *packet_len);
  if (!packet) return NULL;

  size_t pos = 0;
  
  packet[pos++] = MQTT_PUBLISH << 4 | 0x02; // QoS 1

  memcpy(packet + pos, var_len_buf, var_len_size);
  pos += var_len_size;

  packet[pos++] = topic_len >> 8;
  packet[pos++] = topic_len & 0xFF;
  memcpy(packet + pos, topic, topic_len);
  pos += topic_len;

  packet[pos++] = packet_id >> 8;
  packet[pos++] = packet_id & 0xFF;

  memcpy(packet + pos, payload, payload_len);

  return packet;
}

static uint8_t*
create_subscribe_packet(const char *topic, size_t *packet_len)
{
  size_t topic_len = strlen(topic);
  uint16_t packet_id = 1; // Fixed packet ID for simplicity

  size_t remaining_len = 2 + 2 + topic_len + 1;

  uint8_t var_len_buf[4];
  size_t var_len_size = encode_variable_length(remaining_len, var_len_buf);

  // Total packet length
  *packet_len = 1 + var_len_size + remaining_len;

  uint8_t *packet = (uint8_t *)picorb_alloc(g_mqtt_client->vm, *packet_len);
  if (!packet) return NULL;

  size_t pos = 0;
  
  packet[pos++] = MQTT_SUBSCRIBE << 4 | 0x02; // Set required flags

  memcpy(packet + pos, var_len_buf, var_len_size);
  pos += var_len_size;

  packet[pos++] = packet_id >> 8;
  packet[pos++] = packet_id & 0xFF;

  packet[pos++] = topic_len >> 8;
  packet[pos++] = topic_len & 0xFF;
  memcpy(packet + pos, topic, topic_len);
  pos += topic_len;

  packet[pos] = 1; // QoS 1 for reliable delivery

  return packet;
}

// TCP callbacks
static err_t
mqtt_recv_cb(void *arg, struct altcp_pcb *pcb, struct pbuf *pbuf, err_t err)
{
  mqtt_client_t *client = (mqtt_client_t *)arg;
  if (!client) return ERR_ARG;

  if (err != ERR_OK) {
    client->state = MQTT_STATE_DISCONNECTED;
    return err;
  }

  if (pbuf != NULL) {
    // Safely allocate and copy data in LwIP context
    char *tmpbuf = picorb_alloc(client->vm, pbuf->tot_len + 1);
    if (!tmpbuf) {
      altcp_recved(pcb, pbuf->tot_len);
      pbuf_free(pbuf);
      return ERR_MEM;
    }

    struct pbuf *current_pbuf = pbuf;
    int offset = 0;

    while (current_pbuf != NULL) {
      pbuf_copy_partial(current_pbuf, tmpbuf + offset, current_pbuf->len, 0);
      offset += current_pbuf->len;
      current_pbuf = current_pbuf->next;
    }
    tmpbuf[pbuf->tot_len] = '\0';

    uint8_t packet_type = (tmpbuf[0] >> 4) & 0x0F;
    uint8_t flags = tmpbuf[0] & 0x0F;
    uint8_t pos = 1;
    size_t remaining_length = 0;
    uint32_t multiplier = 1;
    uint8_t digit;

    do {
      if (pos >= pbuf->tot_len) {
        picorb_free(client->vm, tmpbuf);
        altcp_recved(pcb, pbuf->tot_len);
        pbuf_free(pbuf);
        return ERR_VAL;
      }
      digit = tmpbuf[pos++];
      remaining_length += (digit & 127) * multiplier;
      multiplier *= 128;
    } while ((digit & 128) != 0);

    switch (packet_type) {
      case MQTT_CONNACK:
        if (pos + 1 < pbuf->tot_len && tmpbuf[pos + 1] == 0) {
          client->state = MQTT_STATE_CONNECTED;
        }
        break;

      case MQTT_PUBLISH:
        {
          // Only buffer the data, processing will happen in main loop
          MQTT_push_event((uint8_t *)tmpbuf, pbuf->tot_len);
        }
        break;

      case MQTT_PUBACK:
        {
          if (remaining_length >= 2 && pos + 1 < pbuf->tot_len) {
            uint16_t packet_id = (tmpbuf[pos] << 8) | tmpbuf[pos + 1];
            (void)packet_id;
          }
        }
        break;

      case MQTT_SUBACK:
        {
          if (remaining_length >= 3 && pos + 2 < pbuf->tot_len) {
            uint16_t packet_id = (tmpbuf[pos] << 8) | tmpbuf[pos + 1];
            uint8_t return_code = tmpbuf[pos + 2];
            (void)packet_id;
            if (return_code > 2) { // 0,1,2 are success codes
              client->state = MQTT_STATE_DISCONNECTED;
              altcp_close(client->pcb);
              client->pcb = NULL;
            }
          }
        }
        break;
    }

    picorb_free(client->vm, tmpbuf);
    altcp_recved(pcb, pbuf->tot_len);
    pbuf_free(pbuf);
  }

  return ERR_OK;
}

static void
mqtt_err_cb(void *arg, err_t err)
{
  mqtt_client_t *client = (mqtt_client_t *)arg;
  if (client) {
    client->state = MQTT_STATE_DISCONNECTED;
  }
}

static err_t
mqtt_connected_cb(void *arg, struct altcp_pcb *pcb, err_t err)
{
  mqtt_client_t *client = (mqtt_client_t *)arg;
  if (!client) return ERR_ARG;

  if (err != ERR_OK) {
    client->state = MQTT_STATE_DISCONNECTED;
    return err;
  }

  size_t packet_len;
  uint8_t *connect_packet = create_connect_packet(client->client_id, &packet_len);
  if (!connect_packet) return ERR_MEM;

  err = altcp_write(pcb, connect_packet, packet_len, TCP_WRITE_FLAG_COPY);
  picorb_free(client->vm, connect_packet);

  if (err != ERR_OK) {
    client->state = MQTT_STATE_DISCONNECTED;
    return err;
  }

  altcp_output(pcb);
  return ERR_OK;
}

bool
MQTT_connect(picorb_state *vm, const char *host, int port, const char *client_id)
{
  // WIP: TLS/SSL support is planned but not yet implemented.
  // Current implementation uses unencrypted connection only.

  if (g_mqtt_client) {
    MQTT_disconnect(vm);
  }

  g_mqtt_client = (mqtt_client_t *)picorb_alloc(vm, sizeof(mqtt_client_t));
  if (!g_mqtt_client) return false;

  g_mqtt_client->host = host;
  g_mqtt_client->port = port;
  g_mqtt_client->client_id = client_id;
  g_mqtt_client->state = MQTT_STATE_DISCONNECTED;
  g_mqtt_client->vm = vm;
  
  g_mqtt_client->packet_buffer = NULL;
  g_mqtt_client->packet_size = 0;
  g_mqtt_client->packet_available = false;
  g_mqtt_client->packet_mutex = false;
  g_mqtt_client->in_callback = false;

  g_mqtt_client->pcb = altcp_new(NULL);
  if (!g_mqtt_client->pcb) {
    picorb_free(vm, g_mqtt_client);
    g_mqtt_client = NULL;
    return false;
  }

  altcp_arg(g_mqtt_client->pcb, g_mqtt_client);
  altcp_recv(g_mqtt_client->pcb, mqtt_recv_cb);
  altcp_err(g_mqtt_client->pcb, mqtt_err_cb);

  ip_addr_t ip;
  ip4_addr_set_zero(&ip);
  Net_get_ip(host, &ip);

  if (!ip4_addr_isloopback(&ip)) {
    lwip_begin();
    err_t err = altcp_connect(g_mqtt_client->pcb, &ip, port, mqtt_connected_cb);
    lwip_end();

    if (err != ERR_OK) {
      altcp_close(g_mqtt_client->pcb);
      picorb_free(vm, g_mqtt_client);
      g_mqtt_client = NULL;
      return false;
    }

    g_mqtt_client->state = MQTT_STATE_CONNECTING;
    return true;
  }

  return false;
}

bool
MQTT_publish(picorb_state *vm, const char *payload, const char *topic)
{
  if (!g_mqtt_client || g_mqtt_client->state != MQTT_STATE_CONNECTED) return false;

  size_t packet_len;
  uint8_t *publish_packet = create_publish_packet(topic, payload, &packet_len);
  if (!publish_packet) return false;

  lwip_begin();
  err_t err = altcp_write(g_mqtt_client->pcb, publish_packet, packet_len, TCP_WRITE_FLAG_COPY);
  if (err == ERR_OK) {
    altcp_output(g_mqtt_client->pcb);
  }
  lwip_end();

  picorb_free(vm, publish_packet);
  return err == ERR_OK;
}

bool
MQTT_subscribe(picorb_state *vm, const char *topic)
{
  if (!g_mqtt_client || g_mqtt_client->state != MQTT_STATE_CONNECTED) return false;

  size_t packet_len;
  uint8_t *subscribe_packet = create_subscribe_packet(topic, &packet_len);
  if (!subscribe_packet) return false;

  lwip_begin();
  err_t err = altcp_write(g_mqtt_client->pcb, subscribe_packet, packet_len, TCP_WRITE_FLAG_COPY);
  if (err == ERR_OK) {
    altcp_output(g_mqtt_client->pcb);
  }
  lwip_end();

  picorb_free(vm, subscribe_packet);
  return err == ERR_OK;
}

bool
MQTT_disconnect(picorb_state *vm)
{
  if (!g_mqtt_client) return false;

  if (g_mqtt_client->pcb) {
    // Send DISCONNECT packet
    uint8_t disconnect_packet[] = {MQTT_DISCONNECT << 4, 0};
    
    lwip_begin();
    altcp_write(g_mqtt_client->pcb, disconnect_packet, 2, TCP_WRITE_FLAG_COPY);
    altcp_output(g_mqtt_client->pcb);
    altcp_close(g_mqtt_client->pcb);
    lwip_end();
  }

  picorb_free(vm, g_mqtt_client);
  g_mqtt_client = NULL;
  return true;
}
