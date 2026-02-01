# Meishi2 keyboard example
# This is a reimplementation of prk_firmware's meishi2 keymap using picoruby-keyboard_layer
#
# Hardware: Meishi2 (2x2 keys)
# - Rows: GPIO 6, 7
# - Cols: GPIO 28, 27
#
# Original PRK firmware keymap: ~/work/prk_firmware/keyboards/prk_meishi2/keymap.rb

require 'keyboard_layer'

include Keycode
include LayerKeycode

# Pin configuration for Meishi2
ROW_PINS = [6, 7]
COL_PINS = [28, 27]

# Initialize keyboard
kb = KeyboardLayer.new(ROW_PINS, COL_PINS, debounce_time: 5)

# Tap threshold: 200ms (PRK uses different thresholds per key, but we use global setting)
kb.tap_threshold_ms = 200

# Special keycode definitions for custom actions
# Since picoruby-keyboard_layer doesn't support Proc directly,
# we use high keycode values that won't conflict with standard keycodes
KC_STATS = 0xF000  # Print allocation stats
KC_BOOT  = 0xF001  # Enter bootsel mode

# Add layers
# Layout:
#  [0,0]          [0,1]
#  SPC/Layer1     STATS
#
#  [1,0]          [1,1]
#  BOOT           ENT/Layer2

kb.add_layer(:default, [
  MO(1, KC_SPC),  KC_STATS,
  KC_BOOT,        MO(2, KC_ENT)
])

# Layer 1: Accessed by holding key[0,0]
kb.add_layer(:layer1, [
  MO(1, KC_SPC),  KC_1,
  KC_2,           MO(2, KC_ENT)
])

# Layer 2: Accessed by holding key[1,1]
kb.add_layer(:layer2, [
  MO(1, KC_SPC),  KC_F1,
  KC_F2,          MO(2, KC_ENT)
])

# Track key state for long press detection
key_press_times = {}
LONG_PRESS_THRESHOLD = 300  # 300ms for STATS and BOOT keys

# Set up key event handler
kb.on_key_event do |event|
  row = event[:row]
  col = event[:col]
  keycode = event[:keycode]
  pressed = event[:pressed]
  key_pos = [row, col]

  # Handle special keycodes
  case keycode
  when KC_STATS
    if pressed
      # Record press time for long press detection
      key_press_times[key_pos] = Machine.board_millis
    else
      # On release, check if it was a tap or long press
      if press_time = key_press_times[key_pos]
        elapsed = Machine.board_millis - press_time
        if elapsed < LONG_PRESS_THRESHOLD
          # Tap: Print allocation stats
          puts "=== PicoRuby Memory Stats ==="
          if defined?(PicoRubyVM)
            PicoRubyVM.print_alloc_stats
          else
            puts "PicoRubyVM.print_alloc_stats not available"
          end
        end
        key_press_times.delete(key_pos)
      end
    end

  when KC_BOOT
    if pressed
      # Record press time for long press detection
      key_press_times[key_pos] = Machine.board_millis
    else
      # On release, check if it was a tap or long press
      if press_time = key_press_times[key_pos]
        elapsed = Machine.board_millis - press_time
        if elapsed < LONG_PRESS_THRESHOLD
          # Tap: Enter bootsel mode
          puts "Entering bootsel mode..."
          # Note: bootsel! method may not be available in all environments
          # In PRK firmware, this reboots into bootloader mode
          Machine.exit(0)  # Fallback: just exit
        end
        key_press_times.delete(key_pos)
      end
    end

  else
    # Normal key handling
    if pressed
      USB::HID.keyboard_send(event[:modifier], keycode)
    else
      USB::HID.keyboard_release
    end
  end
end

# Start keyboard scanning
puts "Meishi2 keyboard starting..."
puts "Key layout:"
puts "  [Space/L1] [Stats]"
puts "  [Boot]     [Enter/L2]"
puts ""
puts "- Tap [0,0]: Space  | Hold [0,0]: Layer 1 (numbers)"
puts "- Tap [0,1]: Print stats (quick press)"
puts "- Tap [1,0]: Bootsel (quick press)"
puts "- Tap [1,1]: Enter  | Hold [1,1]: Layer 2 (function keys)"
kb.start
