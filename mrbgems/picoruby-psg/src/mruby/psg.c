/*
 * picoruby-psg/src/psg.c
 */

#include "../include/psg.h"

#include "picoruby.h"
#include "mruby/presym.h"
#include "mruby/class.h"
#include "mruby/array.h"
#include "mruby/data.h"
#include "mruby/hash.h"
#include "mruby/variable.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define PSG_PERIOD_Q8_SCALE 256.0
#define PSG_TONE_K ((double)CHIP_CLOCK / 32.0)

static uint32_t psg_period_q8[128];
static double psg_a4_frequency = 440.0;

#define PSG_DRUM_CHANNEL 9
#define PSG_SOUND_MAX_STEPS 16
#define PSG_SOUND_TONE  1
#define PSG_SOUND_NOISE 2

typedef struct {
  uint16_t delay;
  uint16_t tone_period; // 0..4095 A4(440Hz)=142, A3(220Hz)=284, A2(110Hz)=568, A1(55Hz)=1136
  uint8_t volume;       // 0..15
  uint8_t noise_period; // 0..31   4000Hz=31, 7812Hz=16, 15625Hz=8, 31250Hz=4, 62500Hz=2, 125000Hz=1
  uint8_t mixer_flags;  // 0..2
} psg_sound_step_t;

typedef struct {
  uint8_t len;
  psg_sound_step_t steps[PSG_SOUND_MAX_STEPS];
} psg_sound_data_t;

static void
mrb_psg_sound_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

static const struct mrb_data_type mrb_psg_sound_type = {
  "PSG::Sound", mrb_psg_sound_free
};

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

/* From https://musescore.org/sites/musescore.org/files/General%20MIDI%20Standard%20Percussion%20Set%20Key%20Map.pdf
  | Key# | Note | Drum Sound         | | Key# | Note | Drum Sound         |
  |------|------|--------------------| |------|------|--------------------|
  |  35  |  B0  | Acoustic Bass Drum | |  59  |  B2  | Ride Cymbal 2      |
  |  36  |  C1  | Bass Drum 1        | |  60  |  C3  | Hi Bongo           |
  |  37  |  C#1 | Side Stick         | |  61  |  C#3 | Low Bongo          |
  |  38  |  D1  | Acoustic Snare     | |  62  |  D3  | Mute Hi Conga      |
  |  39  |  Eb1 | Hand Clap          | |  63  |  Eb3 | Open Hi Conga      |
  |  40  |  E1  | Electric Snare     | |  64  |  E3  | Low Conga          |
  |  41  |  F1  | Low Floor Tom      | |  65  |  F3  | High Timbale       |
  |  42  |  F#1 | Closed Hi Hat      | |  66  |  F#3 | Low Timbale        |
  |  43  |  G1  | High Floor Tom     | |  67  |  G3  | High Agogo         |
  |  44  |  Ab1 | Pedal Hi-Hat       | |  68  |  Ab3 | Low Agogo          |
  |  45  |  A1  | Low Tom            | |  69  |  A3  | Cabasa             |
  |  46  |  Bb1 | Open Hi-Hat        | |  70  |  Bb3 | Maracas            |
  |  47  |  B1  | Low-Mid Tom        | |  71  |  B3  | Short Whistle      |
  |  48  |  C2  | Hi Mid Tom         | |  72  |  C4  | Long Whistle       |
  |  49  |  C#2 | Crash Cymbal 1     | |  73  |  C#4 | Short Guiro        |
  |  50  |  D2  | High Tom           | |  74  |  D4  | Long Guiro         |
  |  51  |  Eb2 | Ride Cymbal 1      | |  75  |  Eb4 | Claves             |
  |  52  |  E2  | Chinese Cymbal     | |  76  |  E4  | Hi Wood Block      |
  |  53  |  F2  | Ride Bell          | |  77  |  F4  | Low Wood Block     |
  |  54  |  F#2 | Tambourine         | |  78  |  F#4 | Mute Cuica         |
  |  55  |  G2  | Splash Cymbal      | |  79  |  G4  | Open Cuica         |
  |  56  |  Ab2 | Cowbell            | |  80  |  Ab4 | Mute Triangle      |
  |  57  |  A2  | Crash Cymbal 2     | |  81  |  A4  | Open Triangle      |
  |  58  |  Bb2 | Vibraslap          |                                       */
static const psg_sound_data_t *
psg_sound_from_value(mrb_state *mrb, mrb_value value, bool raise_unknown)
{
  const psg_sound_data_t *data = (const psg_sound_data_t *)mrb_data_check_get_ptr(mrb, value, &mrb_psg_sound_type);
  if (data) {
    return data;
  }

  struct RClass *module_PSG = mrb_module_get_id(mrb, MRB_SYM(PSG));
  mrb_value module_value = mrb_obj_value(module_PSG);
  mrb_value name;
  if (mrb_symbol_p(value)) {
    name = value;
  } else if (mrb_integer_p(value)) {
    mrb_value drum_sound_map = mrb_iv_get(mrb, module_value, MRB_IVSYM(drum_sound_map));
    name = mrb_hash_get(mrb, drum_sound_map, value);
    if (mrb_nil_p(name)) {
      return NULL;
    }
  } else {
    mrb_raisef(mrb, E_TYPE_ERROR, "PSG sound must be a Symbol, Integer, or PSG::Sound: %S", value);
  }

  mrb_value sound_table = mrb_iv_get(mrb, module_value, MRB_IVSYM(sound_table));
  mrb_value sound = mrb_hash_get(mrb, sound_table, name);
  if (mrb_nil_p(sound)) {
    if (raise_unknown) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "Unknown PSG sound: %S", name);
    }
    return NULL;
  }
  return (const psg_sound_data_t *)mrb_data_get_ptr(mrb, sound, &mrb_psg_sound_type);
}

static void
psg_sound_data_parse(mrb_state *mrb, mrb_value steps_value, psg_sound_data_t *data)
{
  mrb_int len = RARRAY_LEN(steps_value);
  if (len <= 0 || PSG_SOUND_MAX_STEPS <= len) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "PSG sound data must have 1..%d steps", PSG_SOUND_MAX_STEPS - 1);
  }

  data->len = (uint8_t)(len + 1);
  uint32_t delay = 0;
  mrb_int i = 0;
  while (i < len) {
    mrb_value step = mrb_ary_ref(mrb, steps_value, i);
    if (!mrb_array_p(step) || RARRAY_LEN(step) != 4) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "PSG sound step must be [tone_period, noise_period, volume, duration_ms]: %S", step);
    }
    mrb_value tone_value = mrb_ary_ref(mrb, step, 0);
    mrb_value noise_value = mrb_ary_ref(mrb, step, 1);
    mrb_value volume_value = mrb_ary_ref(mrb, step, 2);
    mrb_value duration_value = mrb_ary_ref(mrb, step, 3);
    if (!mrb_integer_p(tone_value) || !mrb_integer_p(noise_value) || !mrb_integer_p(volume_value) || !mrb_integer_p(duration_value)) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "PSG sound step values must be integers: %S", step);
    }
    mrb_int tone_period = mrb_integer(tone_value);
    mrb_int noise_period = mrb_integer(noise_value);
    mrb_int volume = mrb_integer(volume_value);
    mrb_int duration = mrb_integer(duration_value);
    if (tone_period < 0 || 0x0FFF < tone_period || noise_period < 0 || 31 < noise_period || volume < 0 || 15 < volume || duration < 0) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid PSG sound step: %S", step);
    }
    if ((uint64_t)UINT16_MAX - delay < (uint64_t)duration) {
      mrb_raisef(mrb, E_ARGUMENT_ERROR, "PSG sound duration is too long: %S", step);
    }
    uint8_t mixer_flags = 0;
    if (0 < volume) {
      if (0 < tone_period) {
        mixer_flags |= PSG_SOUND_TONE;
      }
      if (0 < noise_period) {
        mixer_flags |= PSG_SOUND_NOISE;
      }
    }
    data->steps[i].delay = (uint16_t)delay;
    data->steps[i].tone_period = (uint16_t)tone_period;
    data->steps[i].volume = (uint8_t)volume;
    data->steps[i].noise_period = (uint8_t)noise_period;
    data->steps[i].mixer_flags = mixer_flags;
    delay += (uint32_t)duration;
    i++;
  }
  data->steps[len].delay = (uint16_t)delay;
  data->steps[len].tone_period = 0;
  data->steps[len].volume = 0;
  data->steps[len].noise_period = 0;
  data->steps[len].mixer_flags = 0;
}

static mrb_value
mrb_psg_sound_data(mrb_state *mrb, mrb_value self)
{
  const psg_sound_data_t *data = (const psg_sound_data_t *)mrb_data_get_ptr(mrb, self, &mrb_psg_sound_type);
  int len = data->len - 1;
  mrb_value ary = mrb_ary_new_capa(mrb, len);
  int i = 0;
  while (i < len) {
    mrb_value step = mrb_ary_new_capa(mrb, 4);
    mrb_ary_push(mrb, step, mrb_fixnum_value(data->steps[i].tone_period));
    mrb_ary_push(mrb, step, mrb_fixnum_value(data->steps[i].noise_period));
    mrb_ary_push(mrb, step, mrb_fixnum_value(data->steps[i].volume));
    mrb_ary_push(mrb, step, mrb_fixnum_value(data->steps[i + 1].delay - data->steps[i].delay));
    mrb_ary_push(mrb, ary, step);
    i++;
  }
  return ary;
}

static mrb_value
mrb_psg_sound_duration(mrb_state *mrb, mrb_value self)
{
  const psg_sound_data_t *data = (const psg_sound_data_t *)mrb_data_get_ptr(mrb, self, &mrb_psg_sound_type);
  return mrb_fixnum_value(data->steps[data->len - 1].delay);
}

static mrb_value
mrb_psg_sound_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value steps_value;
  mrb_get_args(mrb, "A", &steps_value);

  psg_sound_data_t parsed;
  psg_sound_data_parse(mrb, steps_value, &parsed);
  psg_sound_data_t *data = (psg_sound_data_t *)mrb_malloc(mrb, sizeof(psg_sound_data_t));
  memcpy(data, &parsed, sizeof(psg_sound_data_t));
  if (DATA_PTR(self)) {
    mrb_free(mrb, DATA_PTR(self));
  }
  mrb_data_init(self, data, &mrb_psg_sound_type);
  return self;
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

static uint16_t
psg_rb_free_slots(void)
{
  psg_cs_token_t t = PSG_enter_critical();
  if (!rb.buf) {
    PSG_exit_critical(t);
    return 0;
  }
  uint16_t used = (rb.head - rb.tail) & PSG_PACKET_QUEUE_MASK;
  uint16_t free_slots = (PSG_PACKET_QUEUE_LEN - 1) - used;
  PSG_exit_critical(t);
  return free_slots;
}

static uint8_t
psg_next_sound_generation(void)
{
  psg_cs_token_t t = PSG_enter_critical();
  psg.sound_generation++;
  if (!psg.sound_generation) {
    psg.sound_generation = 1;
  }
  uint8_t generation = psg.sound_generation;
  PSG_exit_critical(t);
  return generation;
}

static mrb_value
mrb_driver_sound_stop(mrb_state *mrb, mrb_value self)
{
  (void)mrb;
  (void)self;
  psg_next_sound_generation();
  return mrb_nil_value();
}

static mrb_value
mrb_driver_sound(mrb_state *mrb, mrb_value self)
{
  mrb_value sound_value;
  mrb_int velocity = 127;
  mrb_int voice = 2;
  mrb_get_args(mrb, "o|ii", &sound_value, &velocity, &voice);
  if (velocity <= 0) {
    return mrb_true_value();
  }
  if (127 < velocity) {
    velocity = 127;
  }
  if (voice < 0 || 2 < voice) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "Invalid PSG sound voice: %d (0-2 expected)", voice);
  }
  const psg_sound_data_t *data = psg_sound_from_value(mrb, sound_value, mrb_symbol_p(sound_value));
  if (!data) {
    return mrb_true_value();
  }

  uint16_t required_slots = data->len;
  if (psg_rb_free_slots() < required_slots) {
    return mrb_false_value();
  }

  uint8_t generation = psg_next_sound_generation();

  int i = 0;
  uint32_t prev_delay = 0;
  while (i < data->len) {
    uint16_t delay = data->steps[i].delay;
    uint32_t tick = prev_delay <= delay ? delay - prev_delay : 0;
    uint8_t volume = (uint8_t)((data->steps[i].volume * velocity + 63) / 127);
    psg_packet_t step = {
      .tick = tick,
      .op   = PSG_PKT_SOUND_STEP,
      .reg  = generation,
      .val  = (uint8_t)(((data->steps[i].mixer_flags & 0x03) << 6) | ((voice & 0x03) << 4) | volume),
      .arg  = data->steps[i].noise_period,
      .aux  = data->steps[i].tone_period,
    };
    if (!PSG_rb_push(&step)) {
      return mrb_false_value();
    }
    prev_delay = delay;
    i++;
  }

  return mrb_true_value();
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
  psg.noise_shift = 0x1FFFF;
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
  struct RClass *class_Sound = mrb_define_class_under_id(mrb, module_PSG, MRB_SYM(Sound), mrb->object_class);
  struct RClass *class_Driver = mrb_define_class_under_id(mrb, module_PSG, MRB_SYM(Driver), mrb->object_class);

  MRB_SET_INSTANCE_TT(class_Sound, MRB_TT_DATA);

  psg_build_equal_tuning(psg_a4_frequency);

  mrb_define_module_function_id(mrb, module_PSG, MRB_SYM(note_to_period), mrb_psg_s_note_to_period, MRB_ARGS_REQ(1) | MRB_ARGS_KEY(1, 0));
  mrb_define_module_function_id(mrb, module_PSG, MRB_SYM(set_tuning), mrb_psg_s_set_tuning, MRB_ARGS_OPT(1) | MRB_ARGS_KEY(1, 0));
  mrb_define_const_id(mrb, module_PSG, MRB_SYM(DRUM_CHANNEL), mrb_fixnum_value(PSG_DRUM_CHANNEL));

  mrb_define_method_id(mrb, class_Sound, MRB_SYM(initialize), mrb_psg_sound_initialize, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, class_Sound, MRB_SYM(data), mrb_psg_sound_data, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, class_Sound, MRB_SYM(duration), mrb_psg_sound_duration, MRB_ARGS_NONE());

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
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(sound), mrb_driver_sound, MRB_ARGS_ARG(1, 2));
  mrb_define_method_id(mrb, class_Driver, MRB_SYM(sound_stop), mrb_driver_sound_stop, MRB_ARGS_NONE());
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
