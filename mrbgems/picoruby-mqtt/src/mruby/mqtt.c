#include "mruby.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/variable.h"

#include "../../include/mqtt.h"

static mrb_value
mrb_mqtt_connect(mrb_state *mrb, mrb_value self)
{
  const char *host, *client_id;
  mrb_int port;
  mrb_get_args(mrb, "ziz", &host, &port, &client_id);

  MQTT_init_context(mrb);

  if (MQTT_connect_impl(host, port, client_id) == 0) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mqtt_subscribe(mrb_state *mrb, mrb_value self)
{
  const char *topic;
  mrb_get_args(mrb, "z", &topic);
  if (MQTT_subscribe_impl(topic) == 0) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mqtt_publish(mrb_state *mrb, mrb_value self)
{
  const char *topic, *payload;
  mrb_get_args(mrb, "zz", &topic, &payload);
  if (MQTT_publish_impl(topic, payload, mrb_strlen(mrb, mrb_str_new_cstr(mrb, payload))) == 0) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_mqtt_disconnect(mrb_state *mrb, mrb_value self)
{
  MQTT_disconnect_impl();
  return mrb_nil_value();
}

static mrb_value
mrb_mqtt_get_message(mrb_state *mrb, mrb_value self)
{
  char *topic, *payload;
  int len = MQTT_get_message_impl(&topic, &payload);
  if (len >= 0) {
    mrb_value ary = mrb_ary_new(mrb);
    mrb_ary_push(mrb, ary, mrb_str_new_cstr(mrb, topic));
    mrb_ary_push(mrb, ary, mrb_str_new(mrb, payload, len));
    return ary;
  } else {
    return mrb_nil_value();
  }
}

void
mrb_picoruby_mqtt_gem_init(mrb_state *mrb)
{
  struct RClass *mqtt_module = mrb_define_module(mrb, "MQTT");
  struct RClass *client_class = mrb_define_class_under(mrb, mqtt_module, "Client", mrb->object_class);

  mrb_define_method(mrb, client_class, "_connect_impl", mrb_mqtt_connect, MRB_ARGS_REQ(3));
  mrb_define_method(mrb, client_class, "_subscribe_impl", mrb_mqtt_subscribe, MRB_ARGS_REQ(1));
  mrb_define_method(mrb, client_class, "_publish_impl", mrb_mqtt_publish, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, client_class, "_disconnect_impl", mrb_mqtt_disconnect, MRB_ARGS_NONE());
  mrb_define_method(mrb, client_class, "_get_message_impl", mrb_mqtt_get_message, MRB_ARGS_NONE());
}

void
mrb_picoruby_mqtt_gem_final(mrb_state *mrb)
{
  MQTT_disconnect_impl();
}
