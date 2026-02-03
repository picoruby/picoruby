/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#include <mruby.h>
#include <mruby/data.h>
#include <mruby/class.h>
#include <mruby/variable.h>
#include <mruby/array.h>
#include <mruby/hash.h>
#include <mruby/presym.h>

#include <string.h>

static void
mrb_keyboard_matrix_free(mrb_state *mrb, void *ptr)
{
  picorb_keyboard_matrix_data *data = (picorb_keyboard_matrix_data*)ptr;
  if (data->keymap) {
    mrb_free(mrb, data->keymap);
  }
  if (data->modifier_map) {
    mrb_free(mrb, data->modifier_map);
  }
  mrb_free(mrb, data);
}

static const struct mrb_data_type mrb_keyboard_matrix_type = {
  "KeyboardMatrix", mrb_keyboard_matrix_free,
};

static mrb_value
mrb_initialize(mrb_state *mrb, mrb_value self)
{
  mrb_value row_pins_ary, col_pins_ary, keymap_ary;
  mrb_value modifier_map_ary = mrb_nil_value();

  mrb_get_args(mrb, "AAA|A", &row_pins_ary, &col_pins_ary, &keymap_ary, &modifier_map_ary);

  picorb_keyboard_matrix_data *data = (picorb_keyboard_matrix_data*)mrb_malloc(mrb, sizeof(picorb_keyboard_matrix_data));
  memset(data, 0, sizeof(picorb_keyboard_matrix_data));

  // Get row pins
  data->row_count = RARRAY_LEN(row_pins_ary);
  for (int i = 0; i < data->row_count && i < 16; i++) {
    data->row_pins[i] = mrb_integer(mrb_ary_ref(mrb, row_pins_ary, i));
  }

  // Get col pins
  data->col_count = RARRAY_LEN(col_pins_ary);
  for (int i = 0; i < data->col_count && i < 16; i++) {
    data->col_pins[i] = mrb_integer(mrb_ary_ref(mrb, col_pins_ary, i));
  }

  // Get keymap
  mrb_int keymap_len = RARRAY_LEN(keymap_ary);
  data->keymap = (uint8_t*)mrb_malloc(mrb, keymap_len);
  for (int i = 0; i < keymap_len; i++) {
    data->keymap[i] = mrb_integer(mrb_ary_ref(mrb, keymap_ary, i));
  }

  // Get modifier map (optional)
  if (!mrb_nil_p(modifier_map_ary)) {
    mrb_int mod_len = RARRAY_LEN(modifier_map_ary);
    data->modifier_map = (uint8_t*)mrb_malloc(mrb, mod_len);
    for (int i = 0; i < mod_len; i++) {
      data->modifier_map[i] = mrb_integer(mrb_ary_ref(mrb, modifier_map_ary, i));
    }
  }

  // Initialize hardware
  data->initialized = keyboard_matrix_init(
    data->row_pins, data->row_count,
    data->col_pins, data->col_count,
    data->keymap, data->modifier_map
  );

  mrb_data_init(self, data, &mrb_keyboard_matrix_type);
  return self;
}

static mrb_value
mrb_scan(mrb_state *mrb, mrb_value self)
{
  picorb_keyboard_matrix_data *data = DATA_PTR(self);

  if (!data || !data->initialized) {
    return mrb_nil_value();
  }

  key_event_t event;
  if (keyboard_matrix_scan(&event)) {
    mrb_value hash = mrb_hash_new_capa(mrb, 5);
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(row)), mrb_fixnum_value(event.row));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(col)), mrb_fixnum_value(event.col));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(keycode)), mrb_fixnum_value(event.keycode));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(modifier)), mrb_fixnum_value(event.modifier));
    mrb_hash_set(mrb, hash, mrb_symbol_value(MRB_SYM(pressed)), mrb_bool_value(event.pressed));
    return hash;
  }

  return mrb_nil_value();
}

static mrb_value
mrb_get_debounce_ms(mrb_state *mrb, mrb_value self)
{
  uint32_t time = keyboard_matrix_get_debounce_ms();
  return mrb_fixnum_value(time);
}

static mrb_value
mrb_set_debounce_ms(mrb_state *mrb, mrb_value self)
{
  mrb_int ms;
  mrb_get_args(mrb, "i", &ms);
  keyboard_matrix_set_debounce_ms(ms);
  return mrb_fixnum_value(ms);
}

void
mrb_picoruby_keyboard_matrix_gem_init(mrb_state *mrb)
{
  struct RClass *kb_matrix_class = mrb_define_class_id(mrb, MRB_SYM(KeyboardMatrix), mrb->object_class);
  MRB_SET_INSTANCE_TT(kb_matrix_class, MRB_TT_DATA);

  mrb_define_method_id(mrb, kb_matrix_class, MRB_SYM(initialize), mrb_initialize, MRB_ARGS_ARG(3, 1));
  mrb_define_method_id(mrb, kb_matrix_class, MRB_SYM(scan), mrb_scan, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, kb_matrix_class, MRB_SYM(debounce_ms), mrb_get_debounce_ms, MRB_ARGS_NONE());
  mrb_define_method_id(mrb, kb_matrix_class, MRB_SYM_E(debounce_ms), mrb_set_debounce_ms, MRB_ARGS_REQ(1));
}

void
mrb_picoruby_keyboard_matrix_gem_final(mrb_state *mrb)
{
  // Nothing to finalize
}
