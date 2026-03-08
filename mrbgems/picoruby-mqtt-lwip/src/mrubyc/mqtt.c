/*
 * MQTT mrubyc bindings
 * lwIP-independent layer - only handles Ruby to C interface
 */

#include <mrubyc.h>

// External function declarations (implementations in ports/rp2040/mqtt.c)
extern int MQTT_connect_impl(const char *host, int port, const char *client_id);
extern int MQTT_publish_impl(const char *topic, const char *payload, int len);
extern int MQTT_subscribe_impl(const char *topic);
extern int MQTT_get_message_impl(char **topic, char **payload);
extern void MQTT_disconnect_impl(void);

// mrubyc C API bindings

static void
c_mqtt_connect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 3) {
    console_printf("[MQTT ERROR] Wrong argument count: expected 3, got %d\n", argc);
    SET_FALSE_RETURN();
    return;
  }

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

  const char *host = (const char *)GET_STRING_ARG(1);
  int port = GET_INT_ARG(2);
  const char *client_id = (const char *)GET_STRING_ARG(3);

  console_printf("[MQTT] C API: connect %s:%d %s\n", host, port, client_id);

  int result = MQTT_connect_impl(host, port, client_id);

  if (result == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mqtt_publish(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 2) {
    console_printf("[MQTT ERROR] Wrong argument count: expected 2, got %d\n", argc);
    SET_FALSE_RETURN();
    return;
  }

  if (v[1].tt != MRBC_TT_STRING || v[2].tt != MRBC_TT_STRING) {
    console_printf("[MQTT ERROR] Arguments must be strings\n");
    SET_FALSE_RETURN();
    return;
  }

  const char *topic = (const char *)GET_STRING_ARG(1);
  const char *payload = (const char *)GET_STRING_ARG(2);

  console_printf("[MQTT] C API: publish %s %s\n", topic, payload);

  int result = MQTT_publish_impl(topic, payload, 0);

  if (result == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mqtt_subscribe(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    console_printf("[MQTT ERROR] Wrong argument count: expected 1, got %d\n", argc);
    SET_FALSE_RETURN();
    return;
  }

  if (v[1].tt != MRBC_TT_STRING) {
    console_printf("[MQTT ERROR] Topic must be string\n");
    SET_FALSE_RETURN();
    return;
  }

  const char *topic = (const char *)GET_STRING_ARG(1);

  console_printf("[MQTT] C API: subscribe %s\n", topic);

  int result = MQTT_subscribe_impl(topic);

  if (result == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_mqtt_get_message(mrbc_vm *vm, mrbc_value v[], int argc)
{
  char *topic, *payload;
  int result = MQTT_get_message_impl(&topic, &payload);

  if (result == 0 && topic && payload) {
    // Return array [topic, payload]
    mrbc_value array = mrbc_array_new(vm, 2);
    mrbc_value topic_val = mrbc_string_new_cstr(vm, topic);
    mrbc_value payload_val = mrbc_string_new_cstr(vm, payload);

    mrbc_array_set(&array, 0, &topic_val);
    mrbc_array_set(&array, 1, &payload_val);

    SET_RETURN(array);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_mqtt_disconnect(mrbc_vm *vm, mrbc_value v[], int argc)
{
  console_printf("[MQTT] C API: disconnect\n");
  MQTT_disconnect_impl();
  SET_NIL_RETURN();
}

void mrbc_mqtt_lwip_init(mrbc_vm *vm) {
  mrbc_define_method(0, mrbc_class_object, "_connect_impl", c_mqtt_connect);
  mrbc_define_method(0, mrbc_class_object, "_publish_impl", c_mqtt_publish);
  mrbc_define_method(0, mrbc_class_object, "_subscribe_impl", c_mqtt_subscribe);
  mrbc_define_method(0, mrbc_class_object, "_get_message_impl", c_mqtt_get_message);
  mrbc_define_method(0, mrbc_class_object, "_disconnect_impl", c_mqtt_disconnect);
}