#include <mruby.h>
#include <mruby/presym.h>
#include <mruby/class.h>
#include <mruby/variable.h>

static mrb_value
mrb_start(mrb_state *mrb, mrb_value self)
{
  mrb_value adc_input = mrb_iv_get(mrb, self, MRB_SYM(adc_input));
  PITCHDETECTOR_start((uint8_t)mrb_fixnum(adc_input));
  return mrb_nil_value();
}

static mrb_value
mrb_volume_threshold_set(mrb_state *mrb, mrb_value self)
{
  mrb_int value;
  mrb_get_args(mrb, "i", &value);
  PITCHDETECTOR_set_volume_threshold((uint16_t)value);
  return mrb_fixnum_value(value);
}

static mrb_value
mrb_stop(mrb_state *mrb, mrb_value self)
{
  PITCHDETECTOR_stop();
  return mrb_nil_value();
}

static mrb_value
mrb_detect_pitch(mrb_state *mrb, mrb_value self)
{
  float pitch = PITCHDETECTOR_detect_pitch();
  if (0.0f < pitch) {
    return mrb_float_value(mrb, pitch);
  } else {
    return mrb_nil_value();
  }
}

void
mrb_picoruby_pitchdetector_gem_init(mrb_state *mrb)
{
  struct RClass *class_PD = mrb_define_class_id(mrb, MRB_SYM(PitchDetector), mrb->object_class);

  mrb_define_method_id(mrb, class_PD, MRB_SYM(start), mrb_start, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_PD, MRB_SYM(stop), mrb_stop, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_PD, MRB_SYM(detect_pitch), mrb_detect_pitch, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_PD, MRB_SYM_E(volume_threshold), mrb_volume_threshold_set, MRB_ARGS_REQ(1));
}


void
mrb_picoruby_pitchdetector_gem_final(mrb_state* mrb)
{
}
