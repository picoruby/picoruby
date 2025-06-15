#include "../../include/mqtt.h"
#include "mrubyc.h"

static void c_mqtt_connect_impl(struct VM *vm, mrbc_value v[], int argc)
{
  const char *host = mrbc_string_cstr(&v[1]);
  int port = mrbc_integer(v[2]);
  const char *client_id = mrbc_string_cstr(&v[3]);

  if (MQTT_connect(vm, host, port, client_id)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void c_mqtt_publish_impl(struct VM *vm, mrbc_value v[], int argc)
{
  const char *payload = mrbc_string_cstr(&v[1]);
  const char *topic = mrbc_string_cstr(&v[2]);

  if (MQTT_publish(vm, payload, topic)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void c_mqtt_subscribe_impl(struct VM *vm, mrbc_value v[], int argc)
{
  const char *topic = mrbc_string_cstr(&v[1]);

  if (MQTT_subscribe(vm, topic)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void c_mqtt_disconnect_impl(struct VM *vm, mrbc_value v[], int argc)
{
  if (MQTT_disconnect(vm)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

// Helper function to check if we're in a callback context
static bool is_in_mqtt_callback(void)
{
  return g_mqtt_client && g_mqtt_client->in_callback;
}

static void c_mqtt_pop_packet_impl(struct VM *vm, mrbc_value v[], int argc)
{
  uint8_t *data;
  uint16_t size;

  if (MQTT_pop_event(&data, &size)) {
    // Now we're in the main loop context, safe to execute Ruby code
    mrbc_value packet = mrbc_string_new(vm, (const char *)data, size);
    picorb_free(vm, data);
    SET_RETURN(packet);
  } else {
    SET_NIL_RETURN();
  }
}

static bool parse_mqtt_packet(const uint8_t *packet, uint16_t packet_size, char **topic, uint16_t *topic_len, char **payload, uint16_t *payload_len)
{
  if (packet_size < 2) return false;

  uint8_t packet_type = (packet[0] >> 4) & 0x0F;
  if (packet_type != MQTT_PUBLISH) return false;

  uint8_t pos = 1;
  size_t remaining_length = 0;
  uint32_t multiplier = 1;
  uint8_t digit;

  do {
    if (pos >= packet_size) return false;
    digit = packet[pos++];
    remaining_length += (digit & 127) * multiplier;
    multiplier *= 128;
  } while ((digit & 128) != 0);

  if (pos + 2 > packet_size) return false;
  *topic_len = (packet[pos] << 8) | packet[pos + 1];
  pos += 2;

  if (pos + *topic_len > packet_size) return false;
  *topic = (char *)packet + pos;
  pos += *topic_len;

  // Skip packet ID if QoS > 0
  uint8_t qos = (packet[0] & 0x06) >> 1;
  if (qos > 0) {
    if (pos + 2 > packet_size) return false;
    pos += 2;
  }

  // Extract payload
  *payload_len = packet_size - pos;
  *payload = (char *)packet + pos;

  return true;
}

static void c_mqtt_parse_packet_impl(struct VM *vm, mrbc_value v[], int argc)
{
  const char *packet = mrbc_string_cstr(&v[1]);
  uint16_t packet_len = mrbc_string_size(&v[1]);

  char *topic, *payload;
  uint16_t topic_len, payload_len;

  if (!parse_mqtt_packet((const uint8_t *)packet, packet_len, &topic, &topic_len, &payload, &payload_len)) {
    SET_NIL_RETURN();
    return;
  }

  mrbc_value result = mrbc_array_new(vm, 2);
  mrbc_value topic_str = mrbc_string_new(vm, topic, topic_len);
  mrbc_value payload_str = mrbc_string_new(vm, payload, payload_len);
  mrbc_array_set(&result, 0, &topic_str);
  mrbc_array_set(&result, 1, &payload_str);
  
  SET_RETURN(result);
}

void mrbc_mqtt_init(void)
{
  mrbc_class *mqtt_module = mrbc_define_module(NULL, "MQTT");
  mrbc_class *mqtt_client = mrbc_define_class_under(NULL, mqtt_module, "Client", mrbc_class_object);

  mrbc_define_method(NULL, mqtt_client, "_connect_impl", c_mqtt_connect_impl);
  mrbc_define_method(NULL, mqtt_client, "_publish_impl", c_mqtt_publish_impl);
  mrbc_define_method(NULL, mqtt_client, "_subscribe_impl", c_mqtt_subscribe_impl);
  mrbc_define_method(NULL, mqtt_client, "_disconnect_impl", c_mqtt_disconnect_impl);
  mrbc_define_method(NULL, mqtt_client, "_pop_packet_impl", c_mqtt_pop_packet_impl);
  mrbc_define_method(NULL, mqtt_client, "_parse_packet_impl", c_mqtt_parse_packet_impl);
}
