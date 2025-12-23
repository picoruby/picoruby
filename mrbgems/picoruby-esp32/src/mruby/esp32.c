#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/class.h>
#include "esp32.h"

static struct RClass *ConnectTimeout;

static mrb_value
c_esp32_wifi_init(mrb_state *mrb, mrb_value self)
{
  if (ESP32_WIFI_init() == 0) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
c_esp32_wifi_initialized(mrb_state *mrb, mrb_value self)
{
  if (ESP32_WIFI_initialized()) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
c_esp32_wifi_connect_timeout(mrb_state *mrb, mrb_value self)
{
  const char *ssid;
  const char *password;
  int auth;
  mrb_int timeout_ms;// = 3 < argc ? GET_INT_ARG(4)*1000 : 60*1000;
  mrb_value timeout;
  mrb_get_args(mrb, "zzi|o", &ssid, &password, &auth, &timeout_ms);
  if (mrb_nil_p(timeout)) {
    timeout_ms = 60 * 1000;
  } else {
    timeout_ms = mrb_fixnum(timeout) * 1000;
  }

  int result = ESP32_WIFI_connect_timeout(ssid, password, auth, timeout_ms);

  if (result == 0) {
    return mrb_true_value();
  } else if (result == -1) {
    return mrb_false_value();
  } else {
    mrb_raise(mrb, ConnectTimeout, "WiFi connection timeout");
    return mrb_nil_value(); // never reached
  }
}

static mrb_value
c_esp32_wifi_disconnect(mrb_state *mrb, mrb_value self)
{
  ESP32_WIFI_disconnect();
  return mrb_true_value();
}

static mrb_value
c_esp32_wifi_tcpip_link_status(mrb_state *mrb, mrb_value self)
{
  int status = ESP32_WIFI_tcpip_link_status();
  return mrb_fixnum_value(status);
}

void
mrb_picoruby_esp32_gem_init(mrb_state *mrb)
{
  struct RClass *class_ESP32 = mrb_define_class_id(mrb, MRB_SYM(ESP32), mrb->object_class);
  
  ConnectTimeout = mrb_define_class_under_id(mrb, class_ESP32, MRB_SYM(ConnectTimeout), E_RUNTIME_ERROR);
  struct RClass *class_WiFi = mrb_define_class_under_id(mrb, class_ESP32, MRB_SYM(WiFi), mrb->object_class);

  mrb_define_class_method_id(mrb, class_WiFi, MRB_SYM(init), c_esp32_wifi_init, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_WiFi, MRB_SYM_Q(initialized), c_esp32_wifi_initialized, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_WiFi, MRB_SYM(connect_timeout), c_esp32_wifi_connect_timeout, MRB_ARGS_ARG(3, 1));
  mrb_define_class_method_id(mrb, class_WiFi, MRB_SYM(disconnect), c_esp32_wifi_disconnect, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_WiFi, MRB_SYM(tcpip_link_status), c_esp32_wifi_tcpip_link_status, MRB_ARGS_NONE());
}

void
mrb_picoruby_esp32_gem_final(mrb_state* mrb)
{
}
