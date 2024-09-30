#include <stdbool.h>
#include <mrubyc.h>
#include "../include/watchdog.h"

static void
c_Watchdog_enable(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc < 1 || 2 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments. expected 1..2");
    return;
  }
  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type. expected Integer");
    return;
  }
  bool pause_on_debug = true;
  if (argc == 2) {
    if (v[2].tt == MRBC_TT_FALSE) {
      pause_on_debug = false;
    } else if (v[2].tt != MRBC_TT_TRUE) {
      mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type. expected Boolean");
      return;
    }
  }
  Watchdog_enable(GET_INT_ARG(1), pause_on_debug);
  SET_INT_RETURN(0);
}

static void
c_Watchdog_disable(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Watchdog_disable();
  SET_INT_RETURN(0);
}

static void
c_Watchdog_reboot(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1 ) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments. expected 1");
    return;
  }
  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type. expected Integer");
    return;
  }
  Watchdog_reboot(GET_INT_ARG(1));
  SET_INT_RETURN(0);
}

/*
 * @params cycles:
 *   This needs to be a divider that when applied to the XOSC input, produces a 1MHz clock.
 *   So if the XOSC is 12MHz, this will need to be 12.
 *   https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpip83c5a856f501a38e7382
 */
static void
c_Watchdog_start_tick(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1 ) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments. expected 1");
    return;
  }
  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type. expected Integer");
    return;
  }
  Watchdog_start_tick(GET_INT_ARG(1));
  SET_INT_RETURN(0);
}

/*
 * alias feed
 */
static void
c_Watchdog_update(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Watchdog_update();
  SET_INT_RETURN(0);
}

static void
c_Watchdog_caused_reboot_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (Watchdog_caused_reboot()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_Watchdog_enable_caused_reboot_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (Watchdog_enable_caused_reboot()) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_Watchdog_get_count(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(Watchdog_get_count());
}

void
mrbc_watchdog_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_Watchdog = mrbc_define_class(0, "Watchdog", mrbc_class_object);

  mrbc_define_method(0, mrbc_class_Watchdog, "enable", c_Watchdog_enable);
  mrbc_define_method(0, mrbc_class_Watchdog, "disable", c_Watchdog_disable);
  mrbc_define_method(0, mrbc_class_Watchdog, "reboot", c_Watchdog_reboot);
  mrbc_define_method(0, mrbc_class_Watchdog, "start_tick", c_Watchdog_start_tick);
  mrbc_define_method(0, mrbc_class_Watchdog, "update", c_Watchdog_update);
  mrbc_define_method(0, mrbc_class_Watchdog, "feed", c_Watchdog_update);
  mrbc_define_method(0, mrbc_class_Watchdog, "caused_reboot?", c_Watchdog_caused_reboot_q);
  mrbc_define_method(0, mrbc_class_Watchdog, "enable_caused_reboot?", c_Watchdog_enable_caused_reboot_q);
  mrbc_define_method(0, mrbc_class_Watchdog, "get_count", c_Watchdog_get_count);
}

