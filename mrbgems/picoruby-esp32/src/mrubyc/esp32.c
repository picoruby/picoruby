#include <mrubyc.h>
#include <string.h>
#include "esp32.h"

static mrbc_class *ConnectTimeout;

static void
c_esp32_wifi_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (ESP32_WIFI_init() == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_esp32_wifi_initialized(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (ESP32_WIFI_initialized()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_esp32_wifi_connect_timeout(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  const char *ssid = (const char *)GET_STRING_ARG(1);
  const char *password = (const char *)GET_STRING_ARG(2);
  int auth = GET_INT_ARG(3);
  int timeout_ms = 30000; // default 30 seconds
  if (argc > 3) {
    timeout_ms = GET_INT_ARG(4);
  }

  int result = ESP32_WIFI_connect_timeout(ssid, password, auth, timeout_ms);

  if (result == 0) {
    SET_TRUE_RETURN();
  } else if (result == -1) {
    SET_FALSE_RETURN();
  } else {
    mrbc_raise(vm, ConnectTimeout, "WiFi connection timeout");
  }
}

static void
c_esp32_wifi_disconnect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  ESP32_WIFI_disconnect();
  SET_TRUE_RETURN();
}

static void
c_esp32_wifi_tcpip_link_status(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int status = ESP32_WIFI_tcpip_link_status();
  SET_INT_RETURN(status);
}

void
mrbc_esp32_init(mrbc_vm *vm)
{
  mrbc_class *class_ESP32 = mrbc_define_class(vm, "ESP32", mrbc_class_object);
  
  ConnectTimeout = mrbc_define_class_under(vm, class_ESP32, "ConnectTimeout", MRBC_CLASS(RuntimeError));

  mrbc_class *class_WiFi = mrbc_define_class_under(vm, class_ESP32, "WiFi", mrbc_class_object);
  mrbc_define_method(vm, class_WiFi, "init", c_esp32_wifi_init);
  mrbc_define_method(vm, class_WiFi, "initialized?", c_esp32_wifi_initialized);
  mrbc_define_method(vm, class_WiFi, "connect_timeout", c_esp32_wifi_connect_timeout);
  mrbc_define_method(vm, class_WiFi, "disconnect", c_esp32_wifi_disconnect);
  mrbc_define_method(vm, class_WiFi, "tcpip_link_status", c_esp32_wifi_tcpip_link_status);
}
