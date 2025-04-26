#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"

static mrb_value
mrb_advertise(mrb_state *mrb, mrb_value self)
{
  mrb_value adv_data;
  mrb_get_args(mrb, "o", &adv_data);

  if (mrb_nil_p(adv_data)) {
    BLE_peripheral_stop_advertise();
  } else if (mrb_string_p(adv_data)) {
    BLE_peripheral_advertise((uint8_t *)RSTRING_PTR(adv_data), RSTRING_LEN(adv_data), false);
  } else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "advertise data must be a string");
  }
  return mrb_fixnum_value(0);
}

void
mrb_init_class_BLE_Broadcaster(mrb_state *mrb, struct RClass *class_BLE)
{
  mrb_define_method_id(mrb, class_BLE, MRB_SYM(broadcaster_advertise), mrb_advertise, MRB_ARGS_REQ(1));
}
