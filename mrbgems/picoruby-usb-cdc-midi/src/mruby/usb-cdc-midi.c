#include <mruby.h>
#include <mruby/class.h>
#include <mruby/presym.h>
#include <mruby/string.h>

#include "../../include/usb-cdc-midi.h"

static mrb_value
mrb_usb_cdc_midi_connected(mrb_state *mrb, mrb_value self)
{
  (void)mrb;
  (void)self;
  return mrb_bool_value(usb_cdc_midi_connected());
}

static mrb_value
mrb_usb_cdc_midi_write(mrb_state *mrb, mrb_value self)
{
  (void)self;
  mrb_value bytes;
  mrb_int offset;
  mrb_get_args(mrb, "Si", &bytes, &offset);
  mrb_int length = RSTRING_LEN(bytes);
  if (offset < 0 || length < offset) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "offset is outside the byte string");
  }
  size_t written = usb_cdc_midi_write(
    (const uint8_t *)RSTRING_PTR(bytes) + offset,
    (size_t)(length - offset)
  );
  return mrb_fixnum_value((mrb_int)written);
}

void
mrb_picoruby_usb_cdc_midi_gem_init(mrb_state *mrb)
{
  struct RClass *usb = mrb_define_module_id(mrb, MRB_SYM(USB));
  struct RClass *cdc = mrb_define_module_under_id(mrb, usb, MRB_SYM(CDC));
  mrb_define_class_method_id(mrb, cdc, MRB_SYM_Q(_midi_connected),
    mrb_usb_cdc_midi_connected, MRB_ARGS_NONE());
  mrb_define_class_method_id(mrb, cdc, MRB_SYM(_midi_write),
    mrb_usb_cdc_midi_write, MRB_ARGS_REQ(2));
}

void
mrb_picoruby_usb_cdc_midi_gem_final(mrb_state *mrb)
{
  (void)mrb;
}
