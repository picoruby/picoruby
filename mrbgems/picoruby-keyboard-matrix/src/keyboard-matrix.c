/*
 * Copyright (c) 2025 HASUMI Hitoshi | MIT License
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../include/keyboard-matrix.h"
#include "../../picoruby-gpio/include/gpio.h"
#include "../../picoruby-machine/include/machine.h"

#define MAX_ROWS 16
#define MAX_COLS 16
#define DEBOUNCE_TIME_MS 5

// Matrix configuration
static uint8_t row_pins[MAX_ROWS];
static uint8_t col_pins[MAX_COLS];
static uint8_t row_count = 0;
static uint8_t col_count = 0;
static const uint8_t *keymap = NULL;
static const uint8_t *modifier_map = NULL;

// Key state tracking
static bool key_state[MAX_ROWS][MAX_COLS];
static uint32_t key_debounce_time[MAX_ROWS][MAX_COLS];
static uint32_t debounce_time_ms = DEBOUNCE_TIME_MS;

// Event queue (simple ring buffer)
#define EVENT_QUEUE_SIZE 16
static key_event_t event_queue[EVENT_QUEUE_SIZE];
static uint8_t queue_head = 0;
static uint8_t queue_tail = 0;

static bool
queue_push(key_event_t *event)
{
  uint8_t next = (queue_head + 1) % EVENT_QUEUE_SIZE;
  if (next == queue_tail) {
    return false;
  }
  event_queue[queue_head] = *event;
  queue_head = next;
  return true;
}

static bool
queue_pop(key_event_t *event)
{
  if (queue_head == queue_tail) {
    return false;
  }
  *event = event_queue[queue_tail];
  queue_tail = (queue_tail + 1) % EVENT_QUEUE_SIZE;
  return true;
}

bool
keyboard_matrix_init(const uint8_t *rows, uint8_t r_count,
                     const uint8_t *cols, uint8_t c_count,
                     const uint8_t *kmap, uint8_t *mod_map)
{
  if (r_count > MAX_ROWS || c_count > MAX_COLS) {
    return false;
  }

  row_count = r_count;
  col_count = c_count;
  keymap = kmap;
  modifier_map = mod_map;

  // Copy pin numbers
  memcpy(row_pins, rows, r_count);
  memcpy(col_pins, cols, c_count);

  // Initialize row pins (outputs, set high)
  for (uint8_t i = 0; i < row_count; i++) {
    GPIO_init(row_pins[i]);
    GPIO_set_dir(row_pins[i], OUT);
    GPIO_write(row_pins[i], 1);
  }

  // Initialize column pins (inputs with pull-up)
  for (uint8_t i = 0; i < col_count; i++) {
    GPIO_init(col_pins[i]);
    GPIO_set_dir(col_pins[i], IN);
    GPIO_pull_up(col_pins[i]);
  }

  // Initialize key state
  memset(key_state, 0, sizeof(key_state));
  memset(key_debounce_time, 0, sizeof(key_debounce_time));

  // Initialize queue
  queue_head = 0;
  queue_tail = 0;

  return true;
}

bool
keyboard_matrix_scan(key_event_t *event)
{
  uint32_t now = Machine_uptime_us() / 1000;

  // Scan matrix
  for (uint8_t row = 0; row < row_count; row++) {
    // Set row low
    GPIO_write(row_pins[row], 0);
    Machine_busy_wait_us(1);

    for (uint8_t col = 0; col < col_count; col++) {
      bool current = !GPIO_read(col_pins[col]);

      // Check if state changed
      if (current != key_state[row][col]) {
        // Debounce check
        if (now - key_debounce_time[row][col] >= debounce_time_ms) {
          key_state[row][col] = current;
          key_debounce_time[row][col] = now;

          // Create event
          key_event_t ev;
          ev.row = row;
          ev.col = col;

          // Get keycode from keymap
          uint16_t index = row * col_count + col;
          ev.keycode = keymap[index];

          // Get modifier if modifier_map is provided
          ev.modifier = modifier_map ? modifier_map[index] : 0;
          ev.pressed = current;

          // Push to queue
          queue_push(&ev);
        }
      }
    }

    // Set row high
    GPIO_write(row_pins[row], 1);
  }

  // Pop event from queue
  return queue_pop(event);
}

uint32_t
keyboard_matrix_get_debounce_time(void)
{
  return debounce_time_ms;
}

void
keyboard_matrix_set_debounce_time(uint32_t ms)
{
  debounce_time_ms = ms;
}

#if defined(PICORB_VM_MRUBY)

#include "mruby/keyboard-matrix.c"

#elif defined(PICORB_VM_MRUBYC)

#include "mrubyc/keyboard-matrix.c"

#endif
