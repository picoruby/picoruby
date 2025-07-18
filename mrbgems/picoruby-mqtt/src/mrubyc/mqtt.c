#include "mrubyc.h"

#include "../../include/mqtt.h"

static void
c_mqtt_connect(mrbc_vm *vm, mrbc_value v[], int argc)
{

  if (argc != 3) {
    console_printf("[MQTT ERROR] Wrong argument count: expected 3, got %d\n", argc);
    SET_FALSE_RETURN();
    return;
  }

                 v[1].tt, MRBC_TT_STRING, v[2].tt, MRBC_TT_INTEGER, v[3].tt, MRBC_TT_STRING);

  if (v[1].tt != MRBC_TT_STRING) {
    console_printf("[MQTT ERROR] Host parameter is not string\n");
    SET_FALSE_RETURN();
    return;
  }

  if (v[2].tt != MRBC_TT_INTEGER) {
    console_printf("[MQTT ERROR] Port parameter is not integer\n");
    SET_FALSE_RETURN();
    return;
  }

  if (v[3].tt != MRBC_TT_STRING) {
    console_printf("[MQTT ERROR] Client_id parameter is not string\n");
    SET_FALSE_RETURN();
    return;
  }

  const char *host = mrbc_string_cstr(&v[1]);
  int port = mrbc_integer(v[2]);
  const char *client_id = mrbc_string_cstr(&v[3]);
  if (!host || !client_id) {
    console_printf("[MQTT ERROR] NULL string parameter: host=%p, client_id=%p\n", host, client_id);
    SET_FALSE_RETURN();
    return;
  }

  MQTT_init_context(vm);

  int result = MQTT_connect_impl(host, port, client_id);

  if (result == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mqtt_subscribe(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *topic = mrbc_string_cstr(&v[1]);
  if (MQTT_subscribe_impl(topic) == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mqtt_publish(mrbc_vm *vm, mrbc_value v[], int argc)
{
  const char *topic = mrbc_string_cstr(&v[1]);
  const char *payload = mrbc_string_cstr(&v[2]);
  if (MQTT_publish_impl(topic, payload, mrbc_string_size(&v[2])) == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mqtt_disconnect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  MQTT_disconnect_impl();
  SET_NIL_RETURN();
}

static void
c_mqtt_get_message(mrbc_vm *vm, mrbc_value v[], int argc)
{
  char *topic, *payload;
  int len = MQTT_get_message_impl(&topic, &payload);
  if (len >= 0) {
    mrbc_value ary = mrbc_array_new(vm, 2);
    mrbc_value topic_val = mrbc_string_new_cstr(vm, topic);
    mrbc_value payload_val = mrbc_string_new(vm, payload, len);
    mrbc_array_set(&ary, 0, &topic_val);
    mrbc_array_set(&ary, 1, &payload_val);
    SET_RETURN(ary);
  } else {
    SET_NIL_RETURN();
  }
}

void
mrbc_mqtt_init(void)
{
  mrbc_class *mqtt_module = mrbc_define_module(0, "MQTT");
  mrbc_class *client_class = mrbc_define_class_under(0, mqtt_module, "Client", mrbc_class_object);

  mrbc_define_method(0, client_class, "_connect_impl", c_mqtt_connect);
  mrbc_define_method(0, client_class, "_subscribe_impl", c_mqtt_subscribe);
  mrbc_define_method(0, client_class, "_publish_impl", c_mqtt_publish);
  mrbc_define_method(0, client_class, "_disconnect_impl", c_mqtt_disconnect);
  mrbc_define_method(0, client_class, "_get_message_impl", c_mqtt_get_message);
}
