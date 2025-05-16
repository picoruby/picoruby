#include "../../include/mqtt.h"
#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"

static bool
parse_mqtt_packet(const uint8_t *packet, uint16_t packet_size, char **topic, uint16_t *topic_len, char **payload, uint16_t *payload_len)
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

  uint8_t qos = (packet[0] & 0x06) >> 1;
  if (qos > 0) {
    if (pos + 2 > packet_size) return false;
    pos += 2;
  }

  *payload_len = packet_size - pos;
  *payload = (char *)packet + pos;

  return true;
}

static mrb_value
mrb_mqtt_connect_impl(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_int port;
  const char *client_id;
  mrb_get_args(mrb, "ziz", &host, &port, &client_id);

  if (MQTT_connect(mrb, host, port, client_id)) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mqtt_publish_impl(mrb_state *mrb, mrb_value self)
{
  const char *payload;
  const char *topic;
  mrb_get_args(mrb, "zz", &payload, &topic);

  if (MQTT_publish(mrb, payload, topic)) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mqtt_subscribe_impl(mrb_state *mrb, mrb_value self)
{
  const char *topic;
  mrb_get_args(mrb, "z", &topic);

  if (MQTT_subscribe(mrb, topic)) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mqtt_disconnect_impl(mrb_state *mrb, mrb_value self)
{
  if (MQTT_disconnect(mrb)) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mqtt_pop_packet_impl(mrb_state *mrb, mrb_value self)
{
  uint8_t *data;
  uint16_t size;

  if (MQTT_pop_event(&data, &size)) {
    mrb_value packet = mrb_str_new(mrb, (const char *)data, size);
    picorb_free(mrb, data);
    return packet;
  }

  return mrb_nil_value();
}

static mrb_value
mrb_mqtt_parse_packet_impl(mrb_state *mrb, mrb_value self)
{
  char *packet;
  mrb_int packet_len;
  mrb_get_args(mrb, "s", &packet, &packet_len);

  char *topic, *payload;
  uint16_t topic_len, payload_len;

  if (!parse_mqtt_packet((uint8_t *)packet, packet_len, &topic, &topic_len, &payload, &payload_len)) {
    return mrb_nil_value();
  }

  mrb_value result = mrb_ary_new(mrb);
  mrb_ary_push(mrb, result, mrb_str_new(mrb, topic, topic_len));
  mrb_ary_push(mrb, result, mrb_str_new(mrb, payload, payload_len));
  
  return result;
}

void
mrb_picoruby_mqtt_gem_init(mrb_state* mrb)
{
  struct RClass *class_MQTTClient = mrb_define_class_id(mrb, MRB_SYM(MQTTClient), mrb->object_class);

  mrb_define_method_id(mrb, class_MQTTClient, MRB_SYM(_connect_impl), mrb_mqtt_connect_impl, MRB_ARGS_REQ(3));
  mrb_define_method_id(mrb, class_MQTTClient, MRB_SYM(_publish_impl), mrb_mqtt_publish_impl, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_MQTTClient, MRB_SYM(_subscribe_impl), mrb_mqtt_subscribe_impl, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_MQTTClient, MRB_SYM(_disconnect_impl), mrb_mqtt_disconnect_impl, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MQTTClient, MRB_SYM(_pop_packet_impl), mrb_mqtt_pop_packet_impl, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_MQTTClient, MRB_SYM(_parse_packet_impl), mrb_mqtt_parse_packet_impl, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_mqtt_gem_final(mrb_state* mrb)
{
}
