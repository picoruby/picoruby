#include <stdbool.h>
#include <mrubyc.h>
#include "../include/cyw43.h"

#define GETIV(str)       mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define WRITE(pin, val)  CYW43_GPIO_write(pin.i, val)

static bool cyw43_arch_init_flag = false;

static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  /*
   * https://www.raspberrypi.com/documentation/pico-sdk/networking.html#CYW43_COUNTRY_
   */
  if (!cyw43_arch_init_flag || GET_ARG(2).tt == MRBC_TT_TRUE) {
    if (CYW43_arch_init_with_country(GET_STRING_ARG(1)) < 0) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CYW43_arch_init_with_country() failed");
      return;
    }
    cyw43_arch_init_flag = true;
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

#ifdef USE_WIFI
static bool cyw43_arch_sta_mode_enabled = false;

static void
c_CYW43_enable_sta_mode(mrbc_vm *vm, mrbc_value *v, int argc)
{
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
c_CYW43_connect_blocking(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (cyw43_arch_sta_mode_enabled && !cyw43_arch_connected) {
    if (CYW43_arch_wifi_connect_blocking(GET_STRING_ARG(1), GET_STRING_ARG(2), GET_INT_ARG(3)) < 0) {
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "CYW43_arch_wifi_connect_blocking() failed");
      return;
    }
    cyw43_arch_connected = true;
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
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
mrbc_cyw43_init(void)
{
  mrbc_class *mrbc_class_CYW43 = mrbc_define_class(0, "CYW43", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_CYW43, "_init", c__init);
  mrbc_define_method(0, mrbc_class_CYW43, "enable_sta_mode", c_CYW43_enable_sta_mode);
  mrbc_define_method(0, mrbc_class_CYW43, "disable_sta_mode", c_CYW43_disable_sta_mode);
  mrbc_define_method(0, mrbc_class_CYW43, "connect_blocking", c_CYW43_connect_blocking);

  mrbc_value *CYW43 = mrbc_get_class_const(mrbc_class_CYW43, mrbc_search_symid("GPIO"));
  mrbc_class *mrbc_class_CYW43_GPIO = CYW43->cls;
  mrbc_define_method(0, mrbc_class_CYW43_GPIO, "write", c_GPIO_write);
  mrbc_define_method(0, mrbc_class_CYW43_GPIO, "read", c_GPIO_read);
}
