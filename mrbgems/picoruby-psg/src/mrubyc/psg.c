/*
 * picoruby-psg/src/mrubyc/psg.c
 */

#include "../include/psg.h"

#include <picoruby.h>
#include <mrubyc.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static void
c_driver_send_reg(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int reg = GET_INT_ARG(1);
  int val = GET_INT_ARG(2);
  int tick_delay = (3 <= argc) ? GET_INT_ARG(3) : 0;
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_REG_WRITE,
    .reg  = (uint8_t)reg,
    .val  = (uint8_t)val,
  };
  if (PSG_rb_push(&p)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
reset_psg(void)
{
  D("PSG: Resetting...");
  if (rb.buf) {
    mrbc_raw_free(rb.buf);
  }
  rb.buf = (psg_packet_t *)mrbc_raw_alloc(sizeof(psg_packet_t) * PSG_PACKET_QUEUE_LEN);
  rb.head = 0;
  rb.tail = 0;
  psg_cs_token_t t = PSG_enter_critical();
  memset(&psg, 0, sizeof(psg));
  psg.r.volume[0] = psg.r.volume[1] = psg.r.volume[2] = 15;
  psg.r.mixer = 0x38;
  psg.mute_mask = 0x07;
  psg.pan[0] = psg.pan[1] = psg.pan[2] = 8;
  PSG_exit_critical(t);
}

static void
c_driver_select_pwm(mrbc_vm *vm, mrbc_value v[], int argc)
{
  psg_drv = &psg_drv_pwm;
  int left  = GET_INT_ARG(1);
  int right = GET_INT_ARG(2);
  console_printf("PSG: PWM left=%d, right=%d\n", left, right);
  reset_psg();
  PSG_tick_start_core1((uint8_t)left, (uint8_t)right);
  SET_NIL_RETURN();
}

static void
c_driver_select_mcp4922(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int ldac = GET_INT_ARG(1);
  psg_drv = &psg_drv_mcp4922;
  reset_psg();
  PSG_tick_start_core1((uint8_t)ldac, 0);
  SET_NIL_RETURN();
}

static void
c_driver_buffer_empty_p(mrbc_vm *vm, mrbc_value v[], int argc)
{
  if (rb.head == rb.tail) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_driver_buffer_flush(mrbc_vm *vm, mrbc_value v[], int argc)
{
  psg_cs_token_t t = PSG_enter_critical();
  rb.head = rb.tail;
  PSG_exit_critical(t);
  SET_NIL_RETURN();
}

static void
c_driver_deinit(mrbc_vm *vm, mrbc_value v[], int argc)
{
  PSG_tick_stop_core1();
  if (rb.buf) {
    mrbc_raw_free(rb.buf);
    rb.buf = NULL;
  }
  SET_NIL_RETURN();
}

/* Set LFO: tr, depth(cent), rate(0.1Hz) */
static void
c_driver_set_lfo(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int tr    = GET_INT_ARG(1);
  int depth = GET_INT_ARG(2);
  int rate  = GET_INT_ARG(3);
  int tick_delay = (4 <= argc) ? GET_INT_ARG(4) : 0;
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_LFO_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)depth,
    .arg  = (uint8_t)rate,
  };
  if (PSG_rb_push(&p)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/* Set PAN */
static void
c_driver_set_pan(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int tr  = GET_INT_ARG(1);
  int pan = GET_INT_ARG(2);
  int tick_delay = (3 <= argc) ? GET_INT_ARG(3) : 0;
  if (tr < 0 || 2 < tr || pan < 0 || 15 < pan) {
    mrbc_raisef(vm, MRBC_CLASS(ArgumentError),
      "Invalid track or pan value: %d, %d", tr, pan);
    return;
  }
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_PAN_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)pan,
  };
  if (PSG_rb_push(&p)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_driver_set_timbre(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int tr     = GET_INT_ARG(1);
  int timbre = GET_INT_ARG(2);
  int tick_delay = (3 <= argc) ? GET_INT_ARG(3) : 0;
  if (tr < 0 || 2 < tr) {
    mrbc_raisef(vm, MRBC_CLASS(ArgumentError),
      "Invalid track: %d (0-2 expected)", tr);
    return;
  }
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_TIMBRE_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)timbre,
  };
  if (PSG_rb_push(&p)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_driver_set_legato(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int tr     = GET_INT_ARG(1);
  int legato = GET_INT_ARG(2);
  int tick_delay = (3 <= argc) ? GET_INT_ARG(3) : 0;
  if (tr < 0 || 2 < tr) {
    mrbc_raisef(vm, MRBC_CLASS(ArgumentError),
      "Invalid track: %d (0-2 expected)", tr);
    return;
  }
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_LEGATO_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)legato,
  };
  if (PSG_rb_push(&p)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/* Mute on/off */
static void
c_driver_mute(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int tr   = GET_INT_ARG(1);
  int flag = GET_INT_ARG(2);
  int tick_delay = (3 <= argc) ? GET_INT_ARG(3) : 0;
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_CH_MUTE,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)flag,
  };
  if (PSG_rb_push(&p)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

/* Direct register write (bypasses ring buffer) */
static void
c_driver_write_reg_direct(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int reg = GET_INT_ARG(1);
  int val = GET_INT_ARG(2);
  PSG_write_reg((uint8_t)reg, (uint8_t)val);
  SET_NIL_RETURN();
}

/* Direct mute (bypasses ring buffer) */
static void
c_driver_mute_direct(mrbc_vm *vm, mrbc_value v[], int argc)
{
  int tr   = GET_INT_ARG(1);
  int flag = GET_INT_ARG(2);
  psg_cs_token_t t = PSG_enter_critical();
  if (flag) psg.mute_mask |=  (1u << tr);
  else      psg.mute_mask &= ~(1u << tr);
  PSG_exit_critical(t);
  SET_NIL_RETURN();
}

void
mrbc_psg_init(mrbc_vm *vm)
{
  mrbc_class *module_PSG = mrbc_define_module(vm, "PSG");
  mrbc_class *class_Driver = mrbc_define_class_under(vm, module_PSG, "Driver", mrbc_class_object);

  mrbc_value chip_clock = mrbc_integer_value(CHIP_CLOCK);
  mrbc_set_class_const(class_Driver, mrbc_str_to_symid("CHIP_CLOCK"), &chip_clock);
  mrbc_value sample_rate = mrbc_integer_value(SAMPLE_RATE);
  mrbc_set_class_const(class_Driver, mrbc_str_to_symid("SAMPLE_RATE"), &sample_rate);

  mrbc_value timbres = mrbc_hash_new(vm, 4);
  mrbc_value key, val;
  key = mrbc_symbol_value(mrbc_str_to_symid("square"));
  val = mrbc_integer_value(PSG_TIMBRE_SQUARE);
  mrbc_hash_set(&timbres, &key, &val);
  key = mrbc_symbol_value(mrbc_str_to_symid("triangle"));
  val = mrbc_integer_value(PSG_TIMBRE_TRIANGLE);
  mrbc_hash_set(&timbres, &key, &val);
  key = mrbc_symbol_value(mrbc_str_to_symid("sawtooth"));
  val = mrbc_integer_value(PSG_TIMBRE_SAWTOOTH);
  mrbc_hash_set(&timbres, &key, &val);
  key = mrbc_symbol_value(mrbc_str_to_symid("invsawtooth"));
  val = mrbc_integer_value(PSG_TIMBRE_INVSAWTOOTH);
  mrbc_hash_set(&timbres, &key, &val);
  mrbc_set_class_const(class_Driver, mrbc_str_to_symid("TIMBRES"), &timbres);

  mrbc_define_method(vm, class_Driver, "select_pwm", c_driver_select_pwm);
  mrbc_define_method(vm, class_Driver, "select_mcp4922", c_driver_select_mcp4922);
  mrbc_define_method(vm, class_Driver, "send_reg", c_driver_send_reg);
  mrbc_define_method(vm, class_Driver, "buffer_empty?", c_driver_buffer_empty_p);
  mrbc_define_method(vm, class_Driver, "buffer_flush", c_driver_buffer_flush);
  mrbc_define_method(vm, class_Driver, "deinit", c_driver_deinit);
  mrbc_define_method(vm, class_Driver, "set_lfo", c_driver_set_lfo);
  mrbc_define_method(vm, class_Driver, "set_pan", c_driver_set_pan);
  mrbc_define_method(vm, class_Driver, "set_timbre", c_driver_set_timbre);
  mrbc_define_method(vm, class_Driver, "set_legato", c_driver_set_legato);
  mrbc_define_method(vm, class_Driver, "mute", c_driver_mute);
  mrbc_define_method(vm, class_Driver, "write_reg_direct", c_driver_write_reg_direct);
  mrbc_define_method(vm, class_Driver, "mute_direct", c_driver_mute_direct);
}
