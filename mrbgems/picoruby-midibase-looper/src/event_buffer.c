#include <mruby.h>
#include <mruby/array.h>
#include <mruby/class.h>
#include <mruby/data.h>
#include <mruby/presym.h>

#include <stdint.h>
#include <string.h>

#define EVENT_RECORD_SIZE 5
#define MIDI_NOTE_OFF 0x80
#define MIDI_NOTE_ON  0x90

typedef struct {
  mrb_int count;
  mrb_int max_events;
  uint8_t data[];
} event_buffer;

static void
event_buffer_free(mrb_state *mrb, void *ptr)
{
  mrb_free(mrb, ptr);
}

static const struct mrb_data_type event_buffer_type = {
  "MIDIBASE::Looper::EventBuffer", event_buffer_free
};

static event_buffer *
get_event_buffer(mrb_state *mrb, mrb_value self)
{
  return (event_buffer *)mrb_data_get_ptr(mrb, self, &event_buffer_type);
}

static mrb_int
integer_in_range(mrb_state *mrb, mrb_value value, mrb_int min, mrb_int max,
                 const char *message)
{
  if (!mrb_integer_p(value)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, message);
  }
  mrb_int integer = mrb_integer(value);
  if (integer < min || max < integer) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, message);
  }
  return integer;
}

static size_t
checked_offset(mrb_state *mrb, event_buffer *buffer, mrb_value index_value)
{
  if (!mrb_integer_p(index_value)) {
    mrb_raise(mrb, E_INDEX_ERROR, "event index out of range");
  }
  mrb_int index = mrb_integer(index_value);
  if (index < 0 || buffer->count <= index) {
    mrb_raise(mrb, E_INDEX_ERROR, "event index out of range");
  }
  return (size_t)index * EVENT_RECORD_SIZE;
}

static int
compare_records(const uint8_t *left, const uint8_t *right)
{
  uint16_t left_tick = (uint16_t)(left[0] | ((uint16_t)left[1] << 8));
  uint16_t right_tick = (uint16_t)(right[0] | ((uint16_t)right[1] << 8));
  if (left_tick < right_tick) return -1;
  if (right_tick < left_tick) return 1;

  mrb_bool left_off = (left[2] & 0xf0) == MIDI_NOTE_OFF;
  mrb_bool right_off = (right[2] & 0xf0) == MIDI_NOTE_OFF;
  if (left_off && !right_off) return -1;
  if (!left_off && right_off) return 1;
  return 0;
}

static mrb_value
event_buffer_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_int max_events;
  mrb_get_args(mrb, "i", &max_events);
  if (max_events <= 0) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "max_events must be a positive Integer");
  }
  if ((size_t)max_events > (SIZE_MAX - sizeof(event_buffer)) / EVENT_RECORD_SIZE) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "max_events is too large");
  }

  size_t size = sizeof(event_buffer) + (size_t)max_events * EVENT_RECORD_SIZE;
  event_buffer *buffer = (event_buffer *)mrb_malloc(mrb, size);
  buffer->count = 0;
  buffer->max_events = max_events;
  void *old_buffer = DATA_PTR(self);
  if (old_buffer) {
    mrb_free(mrb, old_buffer);
  }
  mrb_data_init(self, buffer, &event_buffer_type);
  return self;
}

static mrb_value
event_buffer_count(mrb_state *mrb, mrb_value self)
{
  return mrb_int_value(mrb, get_event_buffer(mrb, self)->count);
}

static mrb_value
event_buffer_max_events(mrb_state *mrb, mrb_value self)
{
  return mrb_int_value(mrb, get_event_buffer(mrb, self)->max_events);
}

static mrb_value
event_buffer_append(mrb_state *mrb, mrb_value self)
{
  mrb_value tick_value, command_value, channel_value, note_value, velocity_value;
  mrb_get_args(mrb, "ooooo", &tick_value, &command_value, &channel_value, &note_value, &velocity_value);
  event_buffer *buffer = get_event_buffer(mrb, self);
  if (buffer->max_events <= buffer->count) {
    return mrb_nil_value();
  }

  mrb_int tick = integer_in_range(mrb, tick_value, 0, UINT16_MAX, "tick must be in 0..65535");
  if (!mrb_symbol_p(command_value)) {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "only Note On/Off can be stored");
  }
  mrb_sym command = mrb_symbol(command_value);
  uint8_t status;
  if (command == MRB_SYM(note_on)) {
    status = MIDI_NOTE_ON;
  }
  else if (command == MRB_SYM(note_off)) {
    status = MIDI_NOTE_OFF;
  }
  else {
    mrb_raise(mrb, E_ARGUMENT_ERROR, "only Note On/Off can be stored");
  }
  mrb_int channel = integer_in_range(mrb, channel_value, 0, 15, "MIDI channel must be in 0..15");
  mrb_int note = integer_in_range(mrb, note_value, 0, 127, "MIDI data must be in 0..127");
  mrb_int velocity = integer_in_range(mrb, velocity_value, 0, 127, "MIDI data must be in 0..127");

  size_t offset = (size_t)buffer->count * EVENT_RECORD_SIZE;
  uint8_t *record = buffer->data + offset;
  record[0] = (uint8_t)(tick & 0xff);
  record[1] = (uint8_t)((tick >> 8) & 0xff);
  record[2] = (uint8_t)(status | channel);
  record[3] = (uint8_t)note;
  record[4] = (uint8_t)velocity;
  buffer->count++;
  return mrb_int_value(mrb, (mrb_int)offset);
}

static mrb_value
event_buffer_tick_at(mrb_state *mrb, mrb_value self)
{
  mrb_value index;
  mrb_get_args(mrb, "o", &index);
  event_buffer *buffer = get_event_buffer(mrb, self);
  size_t offset = checked_offset(mrb, buffer, index);
  uint16_t tick = (uint16_t)(buffer->data[offset] |
                             ((uint16_t)buffer->data[offset + 1] << 8));
  return mrb_fixnum_value(tick);
}

static mrb_value
event_buffer_set_tick_at(mrb_state *mrb, mrb_value self)
{
  mrb_value index, tick_value;
  mrb_get_args(mrb, "oo", &index, &tick_value);
  mrb_int tick = integer_in_range(mrb, tick_value, 0, UINT16_MAX,
                                  "tick must be in 0..65535");
  event_buffer *buffer = get_event_buffer(mrb, self);
  size_t offset = checked_offset(mrb, buffer, index);
  buffer->data[offset] = (uint8_t)(tick & 0xff);
  buffer->data[offset + 1] = (uint8_t)((tick >> 8) & 0xff);
  return mrb_int_value(mrb, tick);
}

static mrb_value
event_buffer_event_at(mrb_state *mrb, mrb_value self)
{
  mrb_value index;
  mrb_get_args(mrb, "o", &index);
  event_buffer *buffer = get_event_buffer(mrb, self);
  size_t offset = checked_offset(mrb, buffer, index);
  uint8_t status = buffer->data[offset + 2];
  mrb_value values[4] = {
    mrb_symbol_value((status & 0xf0) == MIDI_NOTE_ON ? MRB_SYM(note_on) : MRB_SYM(note_off)),
    mrb_fixnum_value(status & 0x0f),
    mrb_fixnum_value(buffer->data[offset + 3]),
    mrb_fixnum_value(buffer->data[offset + 4])
  };
  return mrb_ary_new_from_values(mrb, 4, values);
}

static mrb_value
event_buffer_sort(mrb_state *mrb, mrb_value self)
{
  event_buffer *buffer = get_event_buffer(mrb, self);
  uint8_t temporary[EVENT_RECORD_SIZE];
  mrb_int i = 1;
  while (i < buffer->count) {
    mrb_int j = i;
    while (0 < j) {
      uint8_t *left = buffer->data + (size_t)j * EVENT_RECORD_SIZE;
      uint8_t *right = left - EVENT_RECORD_SIZE;
      if (0 <= compare_records(left, right)) break;
      memcpy(temporary, left, EVENT_RECORD_SIZE);
      memcpy(left, right, EVENT_RECORD_SIZE);
      memcpy(right, temporary, EVENT_RECORD_SIZE);
      j--;
    }
    i++;
  }
  return self;
}

static mrb_value
event_buffer_clear(mrb_state *mrb, mrb_value self)
{
  get_event_buffer(mrb, self)->count = 0;
  return self;
}

void
mrb_picoruby_midibase_looper_gem_init(mrb_state *mrb)
{
  struct RClass *midibase = mrb_module_get_id(mrb, MRB_SYM(MIDIBASE));
  struct RClass *looper = mrb_define_class_under_id(mrb, midibase, MRB_SYM(Looper),
                                                    mrb->object_class);
  struct RClass *event_buffer_class = mrb_define_class_under_id(
    mrb, looper, MRB_SYM(EventBuffer), mrb->object_class
  );
  MRB_SET_INSTANCE_TT(event_buffer_class, MRB_TT_DATA);

  mrb_define_const_id(mrb, event_buffer_class, MRB_SYM(RECORD_SIZE), mrb_fixnum_value(EVENT_RECORD_SIZE));
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM(initialize), event_buffer_initialize, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM(count), event_buffer_count, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM(max_events), event_buffer_max_events, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM(append), event_buffer_append, MRB_ARGS_REQ(5));
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM(tick_at), event_buffer_tick_at, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM(set_tick_at), event_buffer_set_tick_at, MRB_ARGS_REQ(2));
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM(event_at), event_buffer_event_at, MRB_ARGS_REQ(1));
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM_B(sort), event_buffer_sort, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, event_buffer_class, MRB_SYM(clear), event_buffer_clear, MRB_ARGS_NONE());
}

void
mrb_picoruby_midibase_looper_gem_final(mrb_state *mrb)
{
  (void)mrb;
}
