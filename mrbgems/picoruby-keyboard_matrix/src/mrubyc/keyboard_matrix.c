/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#include <mrubyc.h>
#include <string.h>

/*
 * Helper function to get keyboard_matrix data pointer from instance->data
 */
static inline picorb_keyboard_matrix_data*
get_data_ptr(mrbc_value *v)
{
  void *data = v[0].instance->data;
  picorb_keyboard_matrix_data **data_ptr = (picorb_keyboard_matrix_data **)data;
  return *data_ptr;
}

static void
c_initialize(mrbc_vm *vm, mrbc_value *v, int argc)
{
  // Expected args: row_pins_ary, col_pins_ary, keymap_ary, [modifier_map_ary]
  if (argc < 3) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mrbc_value row_pins_ary = GET_ARG(1);
  mrbc_value col_pins_ary = GET_ARG(2);
  mrbc_value keymap_ary = GET_ARG(3);
  mrbc_value modifier_map_ary = (argc >= 4) ? GET_ARG(4) : mrbc_nil_value();

  if (row_pins_ary.tt != MRBC_TT_ARRAY || col_pins_ary.tt != MRBC_TT_ARRAY ||
      keymap_ary.tt != MRBC_TT_ARRAY) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "arguments must be arrays");
    return;
  }

  // Allocate data structure
  picorb_keyboard_matrix_data *data = (picorb_keyboard_matrix_data*)mrbc_raw_alloc(sizeof(picorb_keyboard_matrix_data));
  if (!data) {
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "memory allocation failed");
    return;
  }
  memset(data, 0, sizeof(picorb_keyboard_matrix_data));

  // Get row pins
  data->row_count = row_pins_ary.array->n_stored;
  if (data->row_count > 16) data->row_count = 16;
  for (int i = 0; i < data->row_count; i++) {
    mrbc_value val = mrbc_array_get(&row_pins_ary, i);
    if (val.tt == MRBC_TT_INTEGER) {
      data->row_pins[i] = (uint8_t)val.i;
    }
  }

  // Get col pins
  data->col_count = col_pins_ary.array->n_stored;
  if (data->col_count > 16) data->col_count = 16;
  for (int i = 0; i < data->col_count; i++) {
    mrbc_value val = mrbc_array_get(&col_pins_ary, i);
    if (val.tt == MRBC_TT_INTEGER) {
      data->col_pins[i] = (uint8_t)val.i;
    }
  }

  // Get keymap
  int keymap_len = keymap_ary.array->n_stored;
  data->keymap = (uint8_t*)mrbc_raw_alloc(keymap_len);
  if (!data->keymap) {
    mrbc_raw_free(data);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "memory allocation failed");
    return;
  }
  for (int i = 0; i < keymap_len; i++) {
    mrbc_value val = mrbc_array_get(&keymap_ary, i);
    if (val.tt == MRBC_TT_INTEGER) {
      data->keymap[i] = (uint8_t)val.i;
    }
  }

  // Get modifier map (optional)
  if (modifier_map_ary.tt == MRBC_TT_ARRAY) {
    int mod_len = modifier_map_ary.array->n_stored;
    data->modifier_map = (uint8_t*)mrbc_raw_alloc(mod_len);
    if (!data->modifier_map) {
      mrbc_raw_free(data->keymap);
      mrbc_raw_free(data);
      mrbc_raise(vm, MRBC_CLASS(RuntimeError), "memory allocation failed");
      return;
    }
    for (int i = 0; i < mod_len; i++) {
      mrbc_value val = mrbc_array_get(&modifier_map_ary, i);
      if (val.tt == MRBC_TT_INTEGER) {
        data->modifier_map[i] = (uint8_t)val.i;
      }
    }
  }

  // Initialize hardware
  data->initialized = keyboard_matrix_init(
    data->row_pins, data->row_count,
    data->col_pins, data->col_count,
    data->keymap, data->modifier_map
  );

  // Create instance with pointer to data structure
  mrbc_value instance = mrbc_instance_new(vm, v->cls, sizeof(picorb_keyboard_matrix_data *));
  picorb_keyboard_matrix_data **data_ptr = (picorb_keyboard_matrix_data **)instance.instance->data;
  *data_ptr = data;

  SET_RETURN(instance);
}

static void
c_scan(mrbc_vm *vm, mrbc_value *v, int argc)
{
  picorb_keyboard_matrix_data *data = get_data_ptr(v);

  if (!data || !data->initialized) {
    SET_NIL_RETURN();
    return;
  }

  key_event_t event;
  if (keyboard_matrix_scan(&event)) {
    mrbc_value hash = mrbc_hash_new(vm, 5);

    mrbc_value key, val;

    // row
    key = mrbc_symbol_value(mrbc_str_to_symid("row"));
    val = mrbc_integer_value(event.row);
    mrbc_hash_set(&hash, &key, &val);

    // col
    key = mrbc_symbol_value(mrbc_str_to_symid("col"));
    val = mrbc_integer_value(event.col);
    mrbc_hash_set(&hash, &key, &val);

    // keycode
    key = mrbc_symbol_value(mrbc_str_to_symid("keycode"));
    val = mrbc_integer_value(event.keycode);
    mrbc_hash_set(&hash, &key, &val);

    // modifier
    key = mrbc_symbol_value(mrbc_str_to_symid("modifier"));
    val = mrbc_integer_value(event.modifier);
    mrbc_hash_set(&hash, &key, &val);

    // pressed
    key = mrbc_symbol_value(mrbc_str_to_symid("pressed"));
    val = event.pressed ? mrbc_true_value() : mrbc_false_value();
    mrbc_hash_set(&hash, &key, &val);

    SET_RETURN(hash);
  } else {
    SET_NIL_RETURN();
  }
}

static void
c_get_debounce_time(mrbc_vm *vm, mrbc_value *v, int argc)
{
  uint32_t time = keyboard_matrix_get_debounce_time();
  SET_INT_RETURN(time);
}

static void
c_set_debounce_time(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  if (GET_TT_ARG(1) != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong type of arguments");
    return;
  }
  uint32_t ms = GET_INT_ARG(1);
  keyboard_matrix_set_debounce_time(ms);
  SET_INT_RETURN(ms);
}

void
mrbc_keyboard_matrix_init(mrbc_vm *vm)
{
  mrbc_class *kb_matrix_class = mrbc_define_class(vm, "KeyboardMatrix", mrbc_class_object);

  mrbc_define_method(vm, kb_matrix_class, "initialize", c_initialize);
  mrbc_define_method(vm, kb_matrix_class, "scan", c_scan);
  mrbc_define_method(vm, kb_matrix_class, "debounce_time", c_get_debounce_time);
  mrbc_define_method(vm, kb_matrix_class, "debounce_time=", c_set_debounce_time);
}
