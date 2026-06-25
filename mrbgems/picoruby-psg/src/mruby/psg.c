/*
 * picoruby-psg/src/psg.c
 */

#include "../include/psg.h"

#include "picoruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/hash.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define PSG_PERIOD_Q8_SCALE 256.0
#define PSG_TONE_K ((double)CHIP_CLOCK / 32.0)

static uint32_t psg_period_q8[128];
static double psg_a4_frequency = 440.0;

static const uint16_t psg_just_major_num[12] = { 1, 16, 9, 6, 5, 4, 45, 3, 8, 5, 9, 15 };
static const uint16_t psg_just_major_den[12] = { 1, 15, 8, 5, 4, 3, 32, 2, 5, 3, 5, 8 };
static const uint16_t psg_just_minor_num[12] = { 1, 16, 10, 6, 5, 4, 64, 3, 8, 5, 9, 15 };
static const uint16_t psg_just_minor_den[12] = { 1, 15, 9, 5, 4, 3, 45, 2, 5, 3, 5, 8 };

static double
psg_equal_frequency(double note, double a4_frequency)
{
  return a4_frequency * pow(2.0, (note - 69.0) / 12.0);
}

static uint32_t
psg_period_q8_from_frequency(double frequency)
{
  return (uint32_t)((PSG_TONE_K / frequency) * PSG_PERIOD_Q8_SCALE + 0.5);
}

static void
psg_build_equal_tuning(double a4_frequency)
{
  int note = 0;
  while (note < 128) {
    psg_period_q8[note] = psg_period_q8_from_frequency(psg_equal_frequency((double)note, a4_frequency));
    note++;
  }
}

static void
psg_build_just_tuning(int tonic, bool minor, double a4_frequency)
{
  const uint16_t *num = minor ? psg_just_minor_num : psg_just_major_num;
  const uint16_t *den = minor ? psg_just_minor_den : psg_just_major_den;
  int tonic_note = 60 + tonic;
  double tonic_frequency = psg_equal_frequency((double)tonic_note, a4_frequency);
  int note = 0;

  while (note < 128) {
    int diff = note - tonic_note;
    int degree = diff % 12;
    if (degree < 0) {
      degree += 12;
    }
    int octave = (diff - degree) / 12;
    double ratio = (double)num[degree] / (double)den[degree];
    double frequency = tonic_frequency * ratio * pow(2.0, (double)octave);
    psg_period_q8[note] = psg_period_q8_from_frequency(frequency);
    note++;
  }
}

static bool
psg_parse_just_tuning(mrb_sym sym, int *tonic, bool *minor)
{
#define PSG_MATCH_TUNING(name, pitch_class, is_minor) \
  if (sym == MRB_SYM(name)) {                         \
    *tonic = (pitch_class);                           \
    *minor = (is_minor);                              \
    return true;                                      \
  }

  PSG_MATCH_TUNING(just_c_major, 0, false)
  PSG_MATCH_TUNING(just_c_sharp_major, 1, false)
  PSG_MATCH_TUNING(just_d_flat_major, 1, false)
  PSG_MATCH_TUNING(just_d_major, 2, false)
  PSG_MATCH_TUNING(just_d_sharp_major, 3, false)
  PSG_MATCH_TUNING(just_e_flat_major, 3, false)
  PSG_MATCH_TUNING(just_e_major, 4, false)
  PSG_MATCH_TUNING(just_f_major, 5, false)
  PSG_MATCH_TUNING(just_f_sharp_major, 6, false)
  PSG_MATCH_TUNING(just_g_flat_major, 6, false)
  PSG_MATCH_TUNING(just_g_major, 7, false)
  PSG_MATCH_TUNING(just_g_sharp_major, 8, false)
  PSG_MATCH_TUNING(just_a_flat_major, 8, false)
  PSG_MATCH_TUNING(just_a_major, 9, false)
  PSG_MATCH_TUNING(just_a_sharp_major, 10, false)
  PSG_MATCH_TUNING(just_b_flat_major, 10, false)
  PSG_MATCH_TUNING(just_b_major, 11, false)
  PSG_MATCH_TUNING(just_c_minor, 0, true)
  PSG_MATCH_TUNING(just_c_sharp_minor, 1, true)
  PSG_MATCH_TUNING(just_d_flat_minor, 1, true)
  PSG_MATCH_TUNING(just_d_minor, 2, true)
  PSG_MATCH_TUNING(just_d_sharp_minor, 3, true)
  PSG_MATCH_TUNING(just_e_flat_minor, 3, true)
  PSG_MATCH_TUNING(just_e_minor, 4, true)
  PSG_MATCH_TUNING(just_f_minor, 5, true)
  PSG_MATCH_TUNING(just_f_sharp_minor, 6, true)
  PSG_MATCH_TUNING(just_g_flat_minor, 6, true)
  PSG_MATCH_TUNING(just_g_minor, 7, true)
  PSG_MATCH_TUNING(just_g_sharp_minor, 8, true)
  PSG_MATCH_TUNING(just_a_flat_minor, 8, true)
  PSG_MATCH_TUNING(just_a_minor, 9, true)
  PSG_MATCH_TUNING(just_a_sharp_minor, 10, true)
  PSG_MATCH_TUNING(just_b_flat_minor, 10, true)
  PSG_MATCH_TUNING(just_b_minor, 11, true)

#undef PSG_MATCH_TUNING
  return false;
}

static mrb_value
mrb_psg_s_set_tuning(mrb_state *mrb, mrb_value klass)
{
  mrb_sym tuning = MRB_SYM(equal);
  mrb_sym kw_names[] = { MRB_SYM(pitch) };
  mrb_value kw_values[1];
  mrb_kwargs kwargs = { 1, 0, kw_names, kw_values, NULL };
  double a4_frequency = 440.0;
  int tonic = 0;
  bool minor = false;
  mrb_get_args(mrb, "|n:", &tuning, &kwargs);

  if (!mrb_undef_p(kw_values[0])) {
    a4_frequency = mrb_as_float(mrb, kw_values[0]);
  }
  if (a4_frequency <= 0.0) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid PSG tuning pitch: %f", a4_frequency);
  }
  psg_a4_frequency = a4_frequency;

  if (tuning == MRB_SYM(equal)) {
    psg_build_equal_tuning(psg_a4_frequency);
  } else if (psg_parse_just_tuning(tuning, &tonic, &minor)) {
    psg_build_just_tuning(tonic, minor, psg_a4_frequency);
  } else {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Unsupported PSG tuning: %S", mrb_symbol_value(tuning));
  }

  return mrb_symbol_value(tuning);
}

static mrb_value
mrb_psg_s_note_to_period(mrb_state *mrb, mrb_value klass)
{
  mrb_float note;
  mrb_sym kw_names[] = { MRB_SYM(round) };
  mrb_value kw_values[1];
  mrb_kwargs kwargs = { 1, 0, kw_names, kw_values, NULL };
  mrb_get_args(mrb, "f:", &note, &kwargs);

  mrb_bool round = true;
  if (!mrb_undef_p(kw_values[0])) {
    round = mrb_bool(kw_values[0]);
  }

  if (0.0 <= note && note <= 127.0) {
    int index = (int)note;
    uint32_t period_q8 = psg_period_q8[index];
    if (note != (double)index && index < 127) {
      period_q8 = (uint32_t)((int32_t)period_q8 + (int32_t)(((int32_t)psg_period_q8[index + 1] - (int32_t)period_q8) * (note - (double)index)));
    }
    if (round) {
      return mrb_fixnum_value((period_q8 + 128) >> 8);
    } else {
      return mrb_fixnum_value(period_q8 >> 8);
    }
  }

  double frequency = psg_equal_frequency(note, psg_a4_frequency);
  double period = PSG_TONE_K / frequency;
  if (round) {
    period += 0.5;
  }
  return mrb_fixnum_value((mrb_int)period);
}

static mrb_value
mrb_driver_send_reg(mrb_state *mrb, mrb_value klass)
{
  mrb_int reg, val;
  mrb_int tick_delay = 0;
  /* Accept 2 or 3 positional arguments:
     (reg, val)             -> immediate (delay=0)
     (reg, val, tick_delay) -> delayed              */
  mrb_get_args(mrb, "ii|i", &reg, &val, &tick_delay);
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_REG_WRITE,
    .reg  = (uint8_t)reg,
    .val  = (uint8_t)val,
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

static void
reset_psg(mrb_state *mrb)
{
  D("PSG: Resetting...");
  if (rb.buf) {
    mrb_free(mrb, rb.buf);
  }
  rb.buf = mrb_malloc(mrb, sizeof(psg_packet_t) * PSG_PACKET_QUEUE_LEN);
  rb.head = 0;
  rb.tail = 0;
  psg_cs_token_t t = PSG_enter_critical();
  memset(&psg, 0, sizeof(psg));
  psg.r.volume[0] = psg.r.volume[1] = psg.r.volume[2] = 15; // max volume. no envelope
  psg.r.mixer = 0x38; // all noise off, all tone on
  psg.mute_mask = 0x07; // all tracks muted. The driver must unmute first!
  psg.pan[0] = psg.pan[1] = psg.pan[2] = 8; // center pan
  PSG_exit_critical(t);
}

static mrb_value
mrb_driver_s_select_pwm(mrb_state *mrb, mrb_value klass)
{
  psg_drv = &psg_drv_pwm;
  mrb_int left, right;
  mrb_get_args(mrb, "ii", &left, &right);
  D("PSG: PWM left=%d, right=%d\n", left, right);
  reset_psg(mrb);
  PSG_tick_start_core1((uint8_t)left, (uint8_t)right);
  return mrb_nil_value();
}

static mrb_value
mrb_driver_s_select_mcp4922(mrb_state *mrb, mrb_value klass)
{
  mrb_int ldac;
  mrb_get_args(mrb, "i", &ldac);
  psg_drv = &psg_drv_mcp4922;
  reset_psg(mrb);
  PSG_tick_start_core1((uint8_t)ldac, 0);
  return mrb_nil_value();
}

//static mrb_value
//mrb_driver_s_select_usbaudio(mrb_state *mrb, mrb_value klass)
//{
//  reset_psg(mrb);
//  PSG_tick_start_core1(0, 0);
//  return mrb_nil_value();
//}

static mrb_value
mrb_driver_buffer_empty_p(mrb_state *mrb, mrb_value self)
{
  if (rb.head == rb.tail) {
    return mrb_true_value();
  } else {
    return mrb_false_value();
  }
}

static mrb_value
mrb_driver_buffer_flush(mrb_state *mrb, mrb_value self)
{
  psg_cs_token_t t = PSG_enter_critical();
  rb.head = rb.tail;
  PSG_exit_critical(t);
  return mrb_nil_value();
}

static mrb_value
mrb_driver_deinit(mrb_state *mrb, mrb_value self)
{
  PSG_tick_stop_core1();
  if (rb.buf) {
    mrb_free(mrb, rb.buf);
    rb.buf = NULL;
  }
  return mrb_nil_value();
}

/* Set LFO: tr, depth(cent), rate(0.1Hz) */
static mrb_value
mrb_driver_set_lfo(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, depth, rate;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "iii|i", &tr, &depth, &rate, &tick_delay);
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_LFO_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)depth,
    .arg  = (uint8_t)rate,
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

/* Set PAN */
static mrb_value
mrb_driver_set_pan(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, pan;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "ii|i", &tr, &pan, &tick_delay);
  if (tr < 0 || tr > 2 || pan < 0 || pan > 15) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid track or pan value: %d, %d", tr, pan);
  }
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_PAN_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)pan,
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

static mrb_value
mrb_driver_set_timbre(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, timbre;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "ii|i", &tr, &timbre, &tick_delay);
  if (tr < 0 || 2 < tr) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid track: %d (0-2 expected)", tr);
  }
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_TIMBRE_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)timbre
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

static mrb_value
mrb_driver_set_legato(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, legato;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "ii|i", &tr, &legato, &tick_delay);
  if (tr < 0 || 2 < tr) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid track: %d (0-2 expected)", tr);
  }
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_LEGATO_SET,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)legato
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

/* Mute on/off */
static mrb_value
mrb_driver_mute(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, flag;
  mrb_int tick_delay = 0;
  mrb_get_args(mrb, "ii|i", &tr, &flag, &tick_delay);
  psg_packet_t p = {
    .tick = (uint32_t)tick_delay,
    .op   = PSG_PKT_CH_MUTE,
    .reg  = (uint8_t)tr,
    .val  = (uint8_t)flag,
  };
  return PSG_rb_push(&p) ? mrb_true_value() : mrb_false_value();
}

/* Direct register write (bypasses ring buffer) */
static mrb_value
mrb_driver_write_reg_direct(mrb_state *mrb, mrb_value self)
{
  mrb_int reg, val;
  mrb_get_args(mrb, "ii", &reg, &val);
  PSG_write_reg((uint8_t)reg, (uint8_t)val);
  return mrb_nil_value();
}

/* Direct mute (bypasses ring buffer) */
static mrb_value
mrb_driver_mute_direct(mrb_state *mrb, mrb_value self)
{
  mrb_int tr, flag;
  mrb_get_args(mrb, "ii", &tr, &flag);
  psg_cs_token_t t = PSG_enter_critical();
  if (flag) psg.mute_mask |=  (1u << tr);
  else      psg.mute_mask &= ~(1u << tr);
  PSG_exit_critical(t);
  return mrb_nil_value();
}

void
mrb_picoruby_psg_gem_init(mrb_state* mrb)
{
  struct RClass *module_PSG = mrb_define_module_id(mrb, MRB_SYM(PSG));
  struct RClass *class_Driver = mrb_define_class_under_id(mrb, module_PSG, MRB_SYM(Driver), mrb->object_class);

  psg_build_equal_tuning(psg_a4_frequency);

  mrb_define_module_function_id(mrb, module_PSG, MRB_SYM(note_to_period), mrb_psg_s_note_to_period, MRB_ARGS_REQ(1) | MRB_ARGS_KEY(1, 0));
  mrb_define_module_function_id(mrb, module_PSG, MRB_SYM(set_tuning), mrb_psg_s_set_tuning, MRB_ARGS_OPT(1) | MRB_ARGS_KEY(1, 0));

  mrb_define_const_id(mrb, class_Driver, MRB_SYM(CHIP_CLOCK), mrb_fixnum_value(CHIP_CLOCK));
  mrb_define_const_id(mrb, class_Driver, MRB_SYM(SAMPLE_RATE), mrb_fixnum_value(SAMPLE_RATE));

  mrb_value timbres = mrb_hash_new(mrb);
  mrb_hash_set(mrb, timbres, mrb_symbol_value(MRB_SYM(square)), mrb_fixnum_value(PSG_TIMBRE_SQUARE));
  mrb_hash_set(mrb, timbres, mrb_symbol_value(MRB_SYM(triangle)), mrb_fixnum_value(PSG_TIMBRE_TRIANGLE));
  mrb_hash_set(mrb, timbres, mrb_symbol_value(MRB_SYM(sawtooth)), mrb_fixnum_value(PSG_TIMBRE_SAWTOOTH));
  mrb_hash_set(mrb, timbres, mrb_symbol_value(MRB_SYM(invsawtooth)), mrb_fixnum_value(PSG_TIMBRE_INVSAWTOOTH));
  mrb_define_const_id(mrb, class_Driver, MRB_SYM(TIMBRES), timbres);

  mrb_define_class_method_id(mrb, class_Driver, MRB_SYM(select_pwm), mrb_driver_s_select_pwm, MRB_ARGS_REQ(2));
  mrb_define_class_method_id(mrb, class_Driver, MRB_SYM(select_mcp4922), mrb_driver_s_select_mcp4922, MRB_ARGS_REQ(1));
//  mrb_define_class_method_id(mrb, class_Driver, MRB_SYM(select_usbaudio), mrb_driver_s_select_usbaudio, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(send_reg), mrb_driver_send_reg, MRB_ARGS_ARG(2, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM_Q(buffer_empty), mrb_driver_buffer_empty_p, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(buffer_flush), mrb_driver_buffer_flush, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(deinit), mrb_driver_deinit, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(set_lfo), mrb_driver_set_lfo, MRB_ARGS_ARG(3, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(set_pan), mrb_driver_set_pan, MRB_ARGS_ARG(2, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(set_timbre), mrb_driver_set_timbre, MRB_ARGS_ARG(2, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(set_legato), mrb_driver_set_legato, MRB_ARGS_ARG(2, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(mute), mrb_driver_mute, MRB_ARGS_ARG(2, 1));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(write_reg_direct), mrb_driver_write_reg_direct, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(mute_direct), mrb_driver_mute_direct, MRB_ARGS_REQ(2));
}

void
mrb_picoruby_psg_gem_final(mrb_state* mrb)
{
}
