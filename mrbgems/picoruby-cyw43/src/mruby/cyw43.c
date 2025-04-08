#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/variable.h>
#include <mruby/string.h>
#include <mruby/class.h>

static bool cyw43_arch_init_flag = false;

static mrb_value
mrb_s__init(mrb_state *mrb, mrb_value klass)
{
  /*
   * https://www.raspberrypi.com/documentation/pico-sdk/networking.html#CYW43_COUNTRY_
   */
  int res = -1;
  mrb_value country;
  mrb_bool force;
  mrb_get_args(mrb, "ob", &country, &force);
  if (cyw43_arch_init_flag && !force) {
    goto init_end;
  }
  if (mrb_string_p(country)) {
    res = CYW43_arch_init_with_country((const uint8_t *)RSTRING_PTR(country));
  } else {
    res = CYW43_arch_init_with_country(NULL);
  }
  if (res != 0) {
    return mrb_false_value();
  }
  cyw43_arch_init_flag = true;
 init_end:
  return mrb_true_value();
}

static mrb_value
mrb_s_initialized_p(mrb_state *mrb, mrb_value klass)
{
  return mrb_bool_value(cyw43_arch_init_flag);
}

#ifdef USE_WIFI
static bool cyw43_arch_sta_mode_enabled = false;

static mrb_value
mrb_s_enable_sta_mode(mrb_state *mrb, mrb_value klass)
{
  if (!cyw43_arch_init_flag) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "CYW43 not initialized");
  }
  if (!cyw43_arch_sta_mode_enabled) {
    CYW43_arch_enable_sta_mode();
    cyw43_arch_sta_mode_enabled = true;
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_s_disable_sta_mode(mrb_state *mrb, mrb_value klass)
{
  if (!cyw43_arch_init_flag) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "CYW43 not initialized");
  }
  if (cyw43_arch_sta_mode_enabled) {
    CYW43_arch_disable_sta_mode();
    cyw43_arch_sta_mode_enabled = false;
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static bool cyw43_arch_connected = false;

static mrb_value
mrb_s_connect_timeout(mrb_state *mrb, mrb_value klass)
{
  if (!cyw43_arch_init_flag) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "CYW43 not initialized");
  }
  const char *ssid;
  const char *pass;
  mrb_int auth;
  mrb_int timeout_ms;// = 3 < argc ? GET_INT_ARG(4)*1000 : 60*1000;
  mrb_value timeout;
  mrb_get_args(mrb, "zzi|o", &ssid, &pass, &auth, &timeout);
  if (mrb_nil_p(timeout)) {
    timeout_ms = 60 * 1000;
  } else {
    timeout_ms = mrb_fixnum(timeout) * 1000;
  }
  if (cyw43_arch_sta_mode_enabled && !cyw43_arch_connected) {
    if (CYW43_arch_wifi_connect_timeout_ms(ssid, pass, auth, timeout_ms) != 0) {
      struct RClass *class_CYW43 = mrb_class_ptr(klass);
      struct RClass *class_ConnectTimeout = mrb_define_class_under_id(mrb, class_CYW43, MRB_SYM(ConnectTimeout), E_RUNTIME_ERROR);
      mrb_raise(mrb, class_ConnectTimeout, "CYW43_arch_wifi_connect_timeout_ms() failed");
    }
    cyw43_arch_connected = true;
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_s_tcpip_link_status(mrb_state *mrb, mrb_value klass)
{
  if (!cyw43_arch_init_flag) {
    mrb_raise(mrb, E_RUNTIME_ERROR, "CYW43 not initialized");
  }
  return mrb_fixnum_value(CYW43_tcpip_link_status());
}
#endif

static mrb_value
mrb_GPIO_write(mrb_state *mrb, mrb_value self)
{
  mrb_value pin = mrb_iv_get(mrb, self, MRB_IVSYM(pin));
  mrb_int val;
  mrb_get_args(mrb, "i", &val);
  CYW43_GPIO_write(mrb_fixnum(pin), val);
  return mrb_fixnum_value(val);
}

static mrb_value
mrb_GPIO_read(mrb_state *mrb, mrb_value self)
{
  mrb_value pin = mrb_iv_get(mrb, self, MRB_IVSYM(pin));
  return mrb_fixnum_value(CYW43_GPIO_read(mrb_fixnum(pin)));
}

void
mrb_picoruby_cyw43_gem_init(mrb_state* mrb)
{
  struct RClass *class_CYW43 = mrb_define_class_id(mrb, MRB_SYM(CYW43), mrb->object_class);
  struct RClass *class_ConnectTimeout = mrb_define_class_under_id(mrb, class_CYW43, MRB_SYM(ConnectTimeout), E_RUNTIME_ERROR);
  (void)class_ConnectTimeout;

  mrb_define_class_method_id(mrb, class_CYW43, MRB_SYM(_init), mrb_s__init, MRB_ARGS_OPT(1));
  mrb_define_class_method_id(mrb, class_CYW43, MRB_SYM_Q(initialized), mrb_s_initialized_p, MRB_ARGS_NONE());
#ifdef USE_WIFI
  mrb_define_class_method_id(mrb, class_CYW43, MRB_SYM(enable_sta_mode), mrb_s_enable_sta_mode, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_CYW43, MRB_SYM(disable_sta_mode), mrb_s_disable_sta_mode, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_CYW43, MRB_SYM(connect_timeout), mrb_s_connect_timeout, MRB_ARGS_ARG(3, 1));
  mrb_define_class_method_id(mrb, class_CYW43, MRB_SYM(tcpip_link_status), mrb_s_tcpip_link_status, MRB_ARGS_NONE());
  mrb_define_const_id(mrb, class_CYW43, MRB_SYM(LINK_DOWN), mrb_fixnum_value(CYW43_CONST_link_down()));
  mrb_define_const_id(mrb, class_CYW43, MRB_SYM(LINK_JOIN), mrb_fixnum_value(CYW43_CONST_link_join()));
  mrb_define_const_id(mrb, class_CYW43, MRB_SYM(LINK_NOIP), mrb_fixnum_value(CYW43_CONST_link_noip()));
  mrb_define_const_id(mrb, class_CYW43, MRB_SYM(LINK_UP), mrb_fixnum_value(CYW43_CONST_link_up()));
  mrb_define_const_id(mrb, class_CYW43, MRB_SYM(LINK_FAIL), mrb_fixnum_value(CYW43_CONST_link_fail()));
  mrb_define_const_id(mrb, class_CYW43, MRB_SYM(LINK_NONET), mrb_fixnum_value(CYW43_CONST_link_nonet()));
  mrb_define_const_id(mrb, class_CYW43, MRB_SYM(LINK_BADAUTH), mrb_fixnum_value(CYW43_CONST_link_badauth()));
#endif

  struct RClass *class_CYW43_GPIO = mrb_define_class_under_id(mrb, class_CYW43, MRB_SYM(GPIO), mrb->object_class);
  mrb_define_method_id(mrb, class_CYW43_GPIO, MRB_SYM(write), mrb_GPIO_write, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_CYW43_GPIO, MRB_SYM(read), mrb_GPIO_read, MRB_ARGS_NONE());
}

void
mrb_picoruby_cyw43_gem_final(mrb_state* mrb)
{
}
