#include <mrubyc.h>
#include "../include/prk-midi.h"

static void
c_midi_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Midi_init();
}

static void
c_midi_stream_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  mrbc_array value = *(GET_ARY_ARG(1).array);
  uint8_t buffer[3];
  for (int i = 0; i < MIDI_WRITE_PACKET_SIZE; i++)
  {
    if (i < value.n_stored) {
      buffer[i] = (uint8_t)mrbc_integer(value.data[i]);
    } else {
      buffer[i] = 0;
    }
  }
  Midi_stream_write(buffer, (uint8_t)value.n_stored);
}

void
mrbc_prk_midi_init(void)
{
  mrbc_class *mrbc_class_Midi = mrbc_define_class(0, "MIDI", mrbc_class_object);
  mrbc_define_method(0, mrbc_class_Midi, "init", c_midi_init);
  mrbc_define_method(0, mrbc_class_Midi, "write", c_midi_stream_write);
}