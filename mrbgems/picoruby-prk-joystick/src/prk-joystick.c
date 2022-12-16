#include <stdbool.h>
#include <mrubyc.h>
#include "../include/prk-joystick.h"

#define MAGNIFY_BASE 1.0

int8_t  Joystick_axes[MAX_ADC_COUNT] = {-1,-1,-1,-1};
uint8_t Joystick_adc_offset[MAX_ADC_COUNT] = {0,0,0,0};
float   Joystick_sensitivity[MAX_ADC_COUNT] = {-1.0,-1.0,-1.0,-1.0};
int8_t  Joystick_drift_suppression = 5;
int8_t  Joystick_drift_suppression_minus = -5;

static void
c_Joystick_drift_suppression_eq(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int value = GET_INT_ARG(1);
  if (value < 0 || 100 < value) {
    console_printf("Invalid argument for drift_suppression=\n");
    SET_FALSE_RETURN();
    return;
  } else {
    Joystick_drift_suppression = value;
    Joystick_drift_suppression_minus = value * (-1);
    SET_TRUE_RETURN();
  }
}

static void
c_Joystick_reset_axes(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Joystick_reset_zero_report();
  for (int i = 0; i < MAX_ADC_COUNT; i++) {
    Joystick_axes[i] = -1;
    Joystick_adc_offset[i] = 0;
  }
  SET_NIL_RETURN();
}

static void
c_Joystick_init_sensitivity(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int8_t adc_ch = GET_INT_ARG(1);
  float val = GET_FLOAT_ARG(2);
  Joystick_sensitivity[adc_ch] = val * MAGNIFY_BASE;
}

static void
c_Joystick_init_axis_offset(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char *axis = (char *)GET_STRING_ARG(1);
  int8_t adc_ch = GET_INT_ARG(2);
  Joystick_adc_gpio_init(adc_ch);
  Joystick_adc_select_input(adc_ch);
  uint16_t offset_sum = 0;
  for (int i = 0; i < 3; i++) {
    sleep_ms(20);
    offset_sum += Joystick_adc_read();
  }
  Joystick_adc_offset[adc_ch] = (uint8_t)((offset_sum / 3) >> 4);
  if (strcmp(axis, "x") == 0) {
    Joystick_axes[adc_ch] = AXIS_INDEX_X;
  } else if (strcmp(axis, "y") == 0) {
    Joystick_axes[adc_ch] = AXIS_INDEX_Y;
  } else if (strcmp(axis, "z") == 0) {
    Joystick_axes[adc_ch] = AXIS_INDEX_Z;
  } else if (strcmp(axis, "rz") == 0) {
    Joystick_axes[adc_ch] = AXIS_INDEX_RZ;
  } else if (strcmp(axis, "rx") == 0) {
    Joystick_axes[adc_ch] = AXIS_INDEX_RX;
  } else if (strcmp(axis, "ry") == 0) {
    Joystick_axes[adc_ch] = AXIS_INDEX_RY;
  } else {
    console_printf("Invalid axis: %s\n", axis);
    SET_FALSE_RETURN();
    return;
  }
  SET_TRUE_RETURN();
}

void
mrbc_prk_joystick_init(void)
{
  mrbc_class *mrbc_class_Joystick = mrbc_define_class(0, "Joystick", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_Joystick, "reset_axes",          c_Joystick_reset_axes);
  mrbc_define_method(0, mrbc_class_Joystick, "init_axis_offset",    c_Joystick_init_axis_offset);
  mrbc_define_method(0, mrbc_class_Joystick, "drift_suppression=",  c_Joystick_drift_suppression_eq);
  mrbc_define_method(0, mrbc_class_Joystick, "init_sensitivity",    c_Joystick_init_sensitivity);
}

