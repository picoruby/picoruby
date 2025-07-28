#include <mrubyc.h>

static void
c_volume_threshold_set(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_int_t volume_threshold = GET_INT_ARG(1);
  PITCHDETECTOR_set_volume_threshold((uint16_t)value);
  SET_INT_RETURN(volume_threshold);
}

static void
c_start(mrbc_vm *vm, mrbc_value v[], int argc)
{
  mrbc_value adc_input = mrbc_instance_getiv(&v[0], mrbc_str_to_symid("adc_input"));
  PITCHDETECTOR_start((uint8_t)adc_input.i);
}

static void
c_stop(mrbc_vm *vm, mrbc_value v[], int argc)
{
  PITCHDETECTOR_stop();
}

static void
c_detect_pitch(mrbc_vm *vm, mrbc_value v[], int argc)
{
  float pitch = PITCHDETECTOR_detect_pitch();
  if (0 < pitch) {
    SET_FLOAT_RETURN(pitch);
  } else {
    SET_NIL_RETURN();
  }
}

void
mrbc_pitchdetector_init(mrbc_vm *vm)
{
  mrbc_class *class_PD = mrbc_define_class(vm, "PitchDetector", mrbc_class_object);

  mrbc_define_method(vm, class_PD, "start", c_start);
  mrbc_define_method(vm, class_PD, "stop", c_stop);
  mrbc_define_method(vm, class_PD, "detect_pitch", c_detect_pitch);
  mrbc_define_method(vm, class_PD, "volume_threshold=", c_volume_threshold_set);
}
