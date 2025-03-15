#include "mruby.h"
#include "mruby/presym.h"
#include "../include/watchdog.h"

static mrb_value
mrb_s_enable(mrb_state *mrb, mrb_value klass)
{
  mrb_int delay_ms;
  mrb_bool pause_on_debug = TRUE;
  mrb_get_args(mrb, "i|b", &delay_ms, &pause_on_debug);
  Watchdog_enable(delay_ms, pause_on_debug);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_s_disable(mrb_state *mrb, mrb_value klass)
{
  Watchdog_disable();
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_s_reboot(mrb_state *mrb, mrb_value klass)
{
  mrb_int delay_ms;
  mrb_get_args(mrb, "i", &delay_ms);
  mrb_warn(mrb, "\nRebooting in %d ms\n", delay_ms);
  Watchdog_reboot(delay_ms);
  return mrb_fixnum_value(0);
}

/*
 * @params cycles:
 *   This needs to be a divider that when applied to the XOSC input, produces a 1MHz clock.
 *   So if the XOSC is 12MHz, this will need to be 12.
 *   https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#rpip83c5a856f501a38e7382
 */
static mrb_value
mrb_s_start_tick(mrb_state *mrb, mrb_value klass)
{
  mrb_int cycle;
  mrb_get_args(mrb, "i", &cycle);
  Watchdog_start_tick(cycle);
  return mrb_fixnum_value(0);
}

/*
 * alias feed
 */
static mrb_value
mrb_s_update(mrb_state *mrb, mrb_value klass)
{
  Watchdog_update();
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_s_caused_reboot_q(mrb_state *mrb, mrb_value klass)
{
  if (Watchdog_caused_reboot()) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_s_enable_caused_reboot_q(mrb_state *mrb, mrb_value klass)
{
  if (Watchdog_enable_caused_reboot()) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_s_get_count(mrb_state *mrb, mrb_value klass)
{
  return mrb_fixnum_value(Watchdog_get_count());
}

void
mrb_picoruby_watchdog_gem_init(mrb_state* mrb)
{
  struct RClass *class_Watchdog = mrb_define_class_id(mrb, MRB_SYM(Watchdog), mrb->object_class);

  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM(enable), mrb_s_enable, MRB_ARGS_ARG(1, 1));
  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM(disable), mrb_s_disable, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM(reboot), mrb_s_reboot, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM(start_tick), mrb_s_start_tick, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM(update), mrb_s_update, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM(feed), mrb_s_update, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM_Q(caused_reboot), mrb_s_caused_reboot_q, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM_Q(enable_caused_reboot), mrb_s_enable_caused_reboot_q, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Watchdog, MRB_SYM(get_count), mrb_s_get_count, MRB_ARGS_NONE());
}

void
mrb_picoruby_watchdog_gem_final(mrb_state* mrb)
{
}
