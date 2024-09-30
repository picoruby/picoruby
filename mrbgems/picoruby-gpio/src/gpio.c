#include <stdbool.h>
#include <mrubyc.h>
#include "../include/gpio.h"

#define GETIV(str)       mrbc_instance_getiv(&v[0], mrbc_str_to_symid(#str))
#define READ(pin)        GPIO_read(pin_num(vm, pin))
#define WRITE(pin, val)  GPIO_write(pin_num(vm, pin), val)

static int
pin_num(mrbc_vm *vm, mrbc_value pin)
{
  switch (mrbc_type(pin)) {
    case MRBC_TT_INTEGER: {
      return pin.i;
    }
    case MRBC_TT_STRING: {
      return GPIO_pin_num_from_char(pin.string->data);
    }
    case MRBC_TT_SYMBOL: {
      return GPIO_pin_num_from_char((const uint8_t *)mrbc_symid_to_str(pin.i));
    }
    default:
      mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Wrong GPIO pin value");
      return -1;
  }
}

/*
 * GPIO._init(pin)
 */
static void
c__init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  int pin_number = pin_num(vm, v[1]);
  if (-1 < pin_number) GPIO_init(pin_number);
  SET_INT_RETURN(0);
}

/*
 * GPIO.set_function_at(pin, function)
 */
static void
c_set_function_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  GPIO_set_function(pin_num(vm, v[1]), GET_INT_ARG(2));
  SET_INT_RETURN(0);
}


/*
 * GPIO.set_dir_at(pin, dir)
 */
static void
c_set_dir_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  GPIO_set_dir(pin_num(vm, v[1]), GET_INT_ARG(2));
  SET_INT_RETURN(0);
}

/*
 * GPIO.open_drain_at(pin)
 */
static void
c_open_drain_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  GPIO_open_drain(pin_num(vm, v[1]));
  SET_INT_RETURN(0);
}

/*
 * GPIO.pull_up_at(pin)
 */
static void
c_pull_up_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  GPIO_pull_up(pin_num(vm, v[1]));
  SET_INT_RETURN(0);
}

/*
 * GPIO.pull_down_at(pin)
 */
static void
c_pull_down_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  GPIO_pull_down(pin_num(vm, v[1]));
  SET_INT_RETURN(0);
}

/*
 * GPIO.high_at?(pin)
 */
static void
c_high_at_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  //if (READ(v[1]) == 0) {
  if (GPIO_read(GET_INT_ARG(1)) == 0) {
    SET_FALSE_RETURN();
  } else {
    SET_TRUE_RETURN();
  }
}

/*
 * GPIO.high?(pin)
 */
static void
c_high_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (READ(GETIV(pin)) == 0) {
    SET_FALSE_RETURN();
  } else {
    SET_TRUE_RETURN();
  }
}

/*
 * GPIO.low_at?(pin)
 */
static void
c_low_at_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (READ(v[1]) == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/*
 * GPIO.low?(pin)
 */
static void
c_low_q(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (READ(GETIV(pin)) == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/*
 * GPIO.read_at(pin)
 */
static void
c_read_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(READ(v[1]));
}

/*
 * GPIO.read(pin)
 */
static void
c_read(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(READ(GETIV(pin)));
}

/*
 * GPIO.write_at(pin, val)
 */
static void
c_write_at(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[2].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Wrong value. 0 and 1 are only valid");
    return;
  }
  WRITE(v[1], GET_INT_ARG(2));
  SET_INT_RETURN(0);
}

/*
 * GPIO#write(val)
 */
static void
c_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (v[1].tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "Wrong value. 0 and 1 are only valid");
    return;
  }
  WRITE(GETIV(pin), GET_INT_ARG(1));
  SET_INT_RETURN(0);
}

#define SET_CLASS_CONST(cls, cst) \
  mrbc_set_class_const(mrbc_class_##cls, mrbc_str_to_symid(#cst), &mrbc_integer_value(cst))

void
mrbc_gpio_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_GPIO = mrbc_define_class(vm, "GPIO", mrbc_class_object);

  SET_CLASS_CONST(GPIO, IN);
  SET_CLASS_CONST(GPIO, OUT);
  SET_CLASS_CONST(GPIO, HIGH_Z);
  SET_CLASS_CONST(GPIO, PULL_UP);
  SET_CLASS_CONST(GPIO, PULL_DOWN);
  SET_CLASS_CONST(GPIO, OPEN_DRAIN);
  SET_CLASS_CONST(GPIO, ALT);

  mrbc_define_method(vm, mrbc_class_GPIO, "_init", c__init);
  mrbc_define_method(vm, mrbc_class_GPIO, "set_function_at", c_set_function_at);
  mrbc_define_method(vm, mrbc_class_GPIO, "set_dir_at", c_set_dir_at);
  mrbc_define_method(vm, mrbc_class_GPIO, "pull_up_at", c_pull_up_at);
  mrbc_define_method(vm, mrbc_class_GPIO, "pull_down_at", c_pull_down_at);
  mrbc_define_method(vm, mrbc_class_GPIO, "open_drain_at", c_open_drain_at);
  mrbc_define_method(vm, mrbc_class_GPIO, "high_at?", c_high_at_q);
  mrbc_define_method(vm, mrbc_class_GPIO, "high?", c_high_q);
  mrbc_define_method(vm, mrbc_class_GPIO, "low_at?", c_low_at_q);
  mrbc_define_method(vm, mrbc_class_GPIO, "low?", c_low_q);
  mrbc_define_method(vm, mrbc_class_GPIO, "read_at", c_read_at);
  mrbc_define_method(vm, mrbc_class_GPIO, "read", c_read);
  mrbc_define_method(vm, mrbc_class_GPIO, "write_at", c_write_at);
  mrbc_define_method(vm, mrbc_class_GPIO, "write", c_write);
}

