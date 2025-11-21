#include <mrubyc.h>

#define GETIV(str)       mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define WRITE(pin, val)  CYW43_GPIO_write(pin.i, val)

static mrbc_class *ConnectTimeout;
static bool cyw43_arch_init_flag = false;

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  /*
   * https://www.raspberrypi.com/documentation/pico-sdk/networking.html#CYW43_COUNTRY_
   */
  int res = -1;
  if (cyw43_arch_init_flag && (argc < 2 || (1 < argc && GET_ARG(2).tt == MRBC_TT_FALSE))) {
    goto init_end;
  }
  if (0 < argc && GET_ARG(1).tt == MRBC_TT_STRING) {
    res = CYW43_arch_init_with_country(GET_STRING_ARG(1));
  } else {
    res = CYW43_arch_init_with_country(NULL);
  }
  if (res != 0) {
    SET_FALSE_RETURN();
    return;
  }
  cyw43_arch_init_flag = true;
 init_end:
  SET_TRUE_RETURN();
  return;
}

static void
c_CYW43_initialized_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_BOOL_RETURN(cyw43_arch_init_flag);
}

#ifdef USE_WIFI
static bool cyw43_arch_sta_mode_enabled = false;

static void
c_CYW43_enable_sta_mode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!cyw43_arch_init_flag) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CYW43 not initialized");
    return;
  }
  if (!cyw43_arch_sta_mode_enabled) {
    CYW43_arch_enable_sta_mode();
    cyw43_arch_sta_mode_enabled = true;
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_CYW43_disable_sta_mode(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!cyw43_arch_init_flag) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CYW43 not initialized");
    return;
  }
  if (cyw43_arch_sta_mode_enabled) {
    CYW43_arch_disable_sta_mode();
    cyw43_arch_sta_mode_enabled = false;
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static bool cyw43_arch_connected = false;

static void
c_CYW43_connect_timeout(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!cyw43_arch_init_flag) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CYW43 not initialized");
    return;
  }
  if (argc < 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  const char *ssid = (const char *)GET_STRING_ARG(1);
  const char *pass = (const char *)GET_STRING_ARG(2);
  int auth = GET_INT_ARG(3);
  int timeout_ms = 3 < argc ? GET_INT_ARG(4)*1000 : 60*1000;
  if (cyw43_arch_sta_mode_enabled && !cyw43_arch_connected) {
    if (CYW43_wifi_connect_with_dhcp(ssid, pass, auth, timeout_ms) != 0) {
      mrbc_raise(vm, ConnectTimeout, "CYW43_wifi_connect_with_dhcp() failed");
      return;
    }
    cyw43_arch_connected = true;
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_CYW43_disconnect(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!cyw43_arch_init_flag) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CYW43 not initialized");
    return;
  }
  int result = CYW43_wifi_disconnect();
  if (result == 0) {
    cyw43_arch_connected = false;
    // Reset STA mode to allow clean reconnection
    if (cyw43_arch_sta_mode_enabled) {
      CYW43_arch_disable_sta_mode();
      cyw43_arch_sta_mode_enabled = false;
    }
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_CYW43_tcpip_link_status(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!cyw43_arch_init_flag) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CYW43 not initialized");
    return;
  }
  SET_INT_RETURN(CYW43_tcpip_link_status());
}

static void
c_CYW43_dhcp_supplied_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (!cyw43_arch_init_flag) {
    SET_FALSE_RETURN();
    return;
  }
  SET_BOOL_RETURN(CYW43_dhcp_supplied());
}

static void
c_CYW43_ipv4_address(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char ip_str[16] = {0};
  if (!CYW43_ipv4_address(ip_str, 16)) {
    SET_NIL_RETURN();
    return;
  }
  SET_RETURN(mrbc_string_new_cstr(vm, ip_str));
}

static void
c_CYW43_ipv4_netmask(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char netmask_str[16] = {0};
  if (!CYW43_ipv4_netmask(netmask_str, 16)) {
    SET_NIL_RETURN();
    return;
  }
  SET_RETURN(mrbc_string_new_cstr(vm, netmask_str));
}

static void
c_CYW43_ipv4_gateway(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char gateway_str[16] = {0};
  if (!CYW43_ipv4_gateway(gateway_str, 16)) {
    SET_NIL_RETURN();
    return;
  }
  SET_RETURN(mrbc_string_new_cstr(vm, gateway_str));
}
#endif

static void
c_GPIO_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  WRITE(GETIV(pin), GET_INT_ARG(1));
  SET_INT_RETURN(0);
}

static void
c_GPIO_read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(CYW43_GPIO_read(GETIV(pin).i));
}

void
mrbc_cyw43_init(mrbc_vm *vm)
{
  mrbc_class *class_CYW43 = mrbc_define_class(vm, "CYW43", mrbc_class_object);
  ConnectTimeout = mrbc_define_class_under(vm, class_CYW43, "ConnectTimeout", MRBC_CLASS(RuntimeError));

  mrbc_define_method(vm, class_CYW43, "_init", c__init);
  mrbc_define_method(vm, class_CYW43, "initialized?", c_CYW43_initialized_q);
#ifdef USE_WIFI
  mrbc_define_method(vm, class_CYW43, "enable_sta_mode", c_CYW43_enable_sta_mode);
  mrbc_define_method(vm, class_CYW43, "disable_sta_mode", c_CYW43_disable_sta_mode);
  mrbc_define_method(vm, class_CYW43, "connect_timeout", c_CYW43_connect_timeout);
  mrbc_define_method(vm, class_CYW43, "disconnect", c_CYW43_disconnect);
  mrbc_define_method(vm, class_CYW43, "tcpip_link_status", c_CYW43_tcpip_link_status);
  mrbc_define_method(vm, class_CYW43, "dhcp_supplied?", c_CYW43_dhcp_supplied_q);
  mrbc_define_method(vm, class_CYW43, "ipv4_address", c_CYW43_ipv4_address);
  mrbc_define_method(vm, class_CYW43, "ipv4_netmask", c_CYW43_ipv4_netmask);
  mrbc_define_method(vm, class_CYW43, "ipv4_gateway", c_CYW43_ipv4_gateway);
  mrbc_set_class_const(class_CYW43, mrbc_str_to_symid("LINK_DOWN"), &mrbc_integer_value(CYW43_CONST_link_down()));
  mrbc_set_class_const(class_CYW43, mrbc_str_to_symid("LINK_JOIN"), &mrbc_integer_value(CYW43_CONST_link_join()));
  mrbc_set_class_const(class_CYW43, mrbc_str_to_symid("LINK_NOIP"), &mrbc_integer_value(CYW43_CONST_link_noip()));
  mrbc_set_class_const(class_CYW43, mrbc_str_to_symid("LINK_UP"), &mrbc_integer_value(CYW43_CONST_link_up()));
  mrbc_set_class_const(class_CYW43, mrbc_str_to_symid("LINK_FAIL"), &mrbc_integer_value(CYW43_CONST_link_fail()));
  mrbc_set_class_const(class_CYW43, mrbc_str_to_symid("LINK_NONET"), &mrbc_integer_value(CYW43_CONST_link_nonet()));
  mrbc_set_class_const(class_CYW43, mrbc_str_to_symid("LINK_BADAUTH"), &mrbc_integer_value(CYW43_CONST_link_badauth()));
#endif

  mrbc_class *mrbc_class_CYW43_GPIO = mrbc_define_class_under(vm, class_CYW43, "GPIO", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_CYW43_GPIO, "write", c_GPIO_write);
  mrbc_define_method(vm, mrbc_class_CYW43_GPIO, "read", c_GPIO_read);
}
