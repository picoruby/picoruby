#include "mruby.h"
#include "mruby/string.h"
#include "mruby/presym.h"

static mrb_value
mrb_s_tud_task(mrb_state *mrb, mrb_value klass)
{
  Machine_tud_task();
  return mrb_nil_value();
}

static mrb_value
mrb_s_tud_mounted_p(mrb_state *mrb, mrb_value klass)
{
  if (Machine_tud_mounted_q()) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_s_delay_ms(mrb_state *mrb, mrb_value klass)
{
  mrb_int ms;
  mrb_get_args(mrb, "i", &ms);
  if (ms < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "delay time must be positive");
  }
  Machine_delay_ms((uint32_t)ms);
  return mrb_fixnum_value(ms);
}

static mrb_value
mrb_s_busy_wait_ms(mrb_state *mrb, mrb_value klass)
{
  mrb_int ms;
  mrb_get_args(mrb, "i", &ms);
  if (ms < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "delay time must be positive");
  }
  Machine_busy_wait_ms(ms);
  return mrb_fixnum_value(ms);
}

static mrb_value
mrb_s_sleep(mrb_state *mrb, mrb_value klass)
{
  mrb_int sec;
  mrb_get_args(mrb, "i", &sec);
  if (sec < 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "delay time must be positive");
  }
  if (24 * 60 * 60 <= sec) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "sleep length must be less than 24 hours (86400 sec)");
  } else if (sec < 2) {
    // Hangs if you attempt to sleep for 1 second.
    mrb_warn(mrb, "Cannot sleep less than 2 sec\n");
  } else {
    mrb_warn(mrb, "Going to sleep %d sec (USC-CDC will not be back)\n", sec);
    Machine_sleep(sec);
  }
  return mrb_fixnum_value(sec);
}

static mrb_value
mrb_s_deep_sleep(mrb_state *mrb, mrb_value klass)
{
  mrb_notimplement(mrb);
  // TODO: Implement
  mrb_int sec;
  mrb_get_args(mrb, "i", &sec);
  mrb_warn(mrb, "Going to deep sleep\n");
  //Machine_deep_sleep(GET_INT_ARG(1), v[2].tt == mrb_TT_TRUE, v[3].tt == mrb_TT_TRUE);
  return mrb_fixnum_value(0);
}

static mrb_value
mrb_s_unique_id(mrb_state *mrb, mrb_value klass)
{
  char id[33] = {0};
  if (Machine_get_unique_id(id)) {
    mrb_value ret = mrb_str_new_cstr(mrb, (const char *)id);
    return ret;
  } else {
    return mrb_nil_value();
  }
}

static mrb_value
mrb_s_read_memory(mrb_state *mrb, mrb_value klass)
{
  mrb_int addr, size;
  mrb_get_args(mrb, "ii", &addr, &size);
  return mrb_str_new(mrb, (const void *)(uintptr_t)addr, size);
}

void
mrb_picoruby_machine_gem_init(mrb_state* mrb)
{
  struct RClass *class_Machine = mrb_define_class_id(mrb, MRB_SYM(Machine), mrb->object_class);

  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(tud_task), mrb_s_tud_task, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM_Q(tud_mounted), mrb_s_tud_mounted_p, MRB_ARGS_NONE());

  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(delay_ms), mrb_s_delay_ms, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(busy_wait_ms), mrb_s_busy_wait_ms, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(sleep), mrb_s_sleep, MRB_ARGS_REQ(1));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(deep_sleep), mrb_s_deep_sleep, MRB_ARGS_REQ(3));
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(unique_id), mrb_s_unique_id, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, class_Machine, MRB_SYM(read_memory), mrb_s_read_memory, MRB_ARGS_REQ(2));
}

void
mrb_picoruby_machine_gem_final(mrb_state* mrb)
{
}
