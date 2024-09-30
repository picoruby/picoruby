#include <mrubyc.h>
#include "../include/prk-sounder.h"

float    Sounder_song_data[SONG_MAX_LEN] = {0};
uint16_t Sounder_song_len[SONG_MAX_LEN] = {0};
uint8_t  Sounder_song_data_index = 0;
uint8_t  Sounder_song_data_len = 0;

static void
c_sounder_init(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Sounder_init(GET_INT_ARG(1));
}

static void
c_sounder_clear_song(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Sounder_song_data_len = 0;
  memset(Sounder_song_len, 0, sizeof(Sounder_song_len));
}

static void
c_sounder_add_note(mrbc_vm *vm, mrbc_value *v, int argc)
{
  float pitch = GET_FLOAT_ARG(1);
  uint16_t duration = GET_INT_ARG(2);

  if (SONG_MAX_LEN == Sounder_song_data_len + 1) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "SONG_MAX_LEN overflow");
    SET_FALSE_RETURN();
    return;
  }
  if (0 < pitch) {
    Sounder_song_data[Sounder_song_data_len] = pitch * SOUNDER_SM_CYCLES;
    Sounder_song_len[Sounder_song_data_len] = pitch * duration / 1000;
  } else {
    Sounder_song_data[Sounder_song_data_len] = 0;
    Sounder_song_len[Sounder_song_data_len] = SOUNDER_DEFAULT_HZ * duration / 1000;
  }
  Sounder_song_data_len++;
  SET_TRUE_RETURN();
}

static void
c_sounder_replay(mrbc_vm *vm, mrbc_value *v, int argc)
{
  Sounder_replay();
}

void
mrbc_prk_sounder_init(mrbc_vm *vm)
{
  mrbc_class *mrbc_class_Sounder = mrbc_define_class(vm, "Sounder", mrbc_class_object);
  mrbc_define_method(vm, mrbc_class_Sounder, "sounder_init",   c_sounder_init);
  mrbc_define_method(vm, mrbc_class_Sounder, "sounder_replay", c_sounder_replay);
  mrbc_define_method(vm, mrbc_class_Sounder, "add_note",       c_sounder_add_note);
  mrbc_define_method(vm, mrbc_class_Sounder, "clear_song",     c_sounder_clear_song);
}
