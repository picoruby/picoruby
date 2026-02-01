/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#ifndef KEYBOARD_MATRIX_DEFINED_H_
#define KEYBOARD_MATRIX_DEFINED_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Key event structure
typedef struct {
  uint8_t row;
  uint8_t col;
  uint8_t keycode;
  uint8_t modifier;
  bool pressed;
} key_event_t;

// Initialize keyboard matrix
bool keyboard_matrix_init(const uint8_t* row_pins, uint8_t row_count,
                          const uint8_t* col_pins, uint8_t col_count,
                          const uint8_t* keymap, uint8_t* modifier_map);

// Scan keyboard matrix (call periodically)
bool keyboard_matrix_scan(key_event_t* event);

// Get debounce time in milliseconds
uint32_t keyboard_matrix_get_debounce_time(void);

// Set debounce time in milliseconds
void keyboard_matrix_set_debounce_time(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif
