require 'keyboard_matrix'
require 'usb/hid'


# Keyboard class provides layer switching functionality
# It wraps KeyboardMatrix and manages multiple layers (keymaps)
class Keyboard
  include USB::HID::Keycode
  include LayerKeycode

  KC_NO = 0x00  # Transparent key - fallthrough to lower layer

  def initialize(row_pins, col_pins, debounce_ms: 5)
    @row_count = row_pins.size
    @col_count = col_pins.size

    # Layer management
    @layers = {}                # layer_name => keymap array
    @layer_names = []           # layer_index => layer_name
    @default_layer = nil        # Default layer name
    @locked_layer = nil         # Toggle locked layer index (or nil)
    @layer_stack = []           # Momentary layer stack (LIFO)
    @momentary_keys = {}        # {[row, col] => layer_index} - track MO keys

    # Tap/hold key management (LT and MT)
    @tap_hold_keys = {}         # {[row, col] => tap_hold_state}
    @tap_threshold_ms = 200     # Tap threshold in milliseconds
    @repush_threshold_ms = 200  # Repush threshold for double-tap-hold
    @keys_pressed = {}          # Track physically pressed keys: {[row, col] => true}
    @active_keys = {}           # Track active keys with their resolved keycode/modifier: {[row, col] => {keycode:, modifier:}}

    # Initialize underlying KeyboardMatrix
    @matrix = KeyboardMatrix.new(row_pins, col_pins)
    @matrix.debounce_ms = debounce_ms

    # User callback for key events
    @callback = nil
  end

  def add_layer(name, keymap)
    expected_size = @row_count * @col_count
    if keymap.size != expected_size
      raise ArgumentError, "Keymap size must be #{expected_size}, got #{keymap.size}"
    end

    @layers[name] = keymap
    @layer_names << name unless @layer_names.include?(name)

    # Set as default layer if it's the first layer
    @default_layer ||= name
  end

  def default_layer=(name)
    unless @layers.has_key?(name)
      raise ArgumentError, "Layer :#{name} does not exist"
    end
    @default_layer = name
  end

  def tap_threshold_ms=(value)
    @tap_threshold_ms = value
  end

  def repush_threshold_ms=(value)
    @repush_threshold_ms = value
  end

  def start(&block)
    @callback = block

    unless @callback
      raise "Callback block is required. Use on_key_event to set a callback."
    end

    loop do
      if event = @matrix.scan
        handle_event(event)
      end
      # Always update LT/MT tap key states, even without key events
      # This ensures tap threshold timeout is properly detected
      update_tap_hold_keys
      sleep_ms(1)
    end
  end

  private

  def is_modifier_key?(keycode)
    # USB HID modifier keys are in the range 0xE0-0xE7
    keycode >= 0xE0 && keycode <= 0xE7
  end

  def handle_event(event)
    row = event[:row]
    col = event[:col]
    pressed = event[:pressed]
    # @type var key_pos: [Integer, Integer]
    key_pos = [row, col]

    # Track physically pressed keys for safety cleanup
    if pressed
      @keys_pressed[key_pos] = true
    else
      @keys_pressed.delete(key_pos)
    end

    # Track other key presses for tap/hold interruption BEFORE resolving keycode
    # This ensures layers/modifiers are activated immediately when another key is pressed
    if pressed
      mark_other_key_pressed
    end

    # Resolve keycode AND modifier from layers
    keycode, modifier = resolve_key(row, col)

    # Handle special layer keycodes
    # Note: MT must be checked before LT because MT_BASE is within LT range
    if is_mt?(keycode)
      modifier_index = mt_modifier_index(keycode)
      tap_keycode = mt_tap_keycode(keycode)
      handle_mt_key(row, col, modifier_index, tap_keycode, pressed)
      # MT keys don't generate key events (handled internally)
      return
    elsif is_mo?(keycode)
      layer_index = mo_layer(keycode)
      handle_mo_key(row, col, layer_index, pressed)
      # MO keys don't generate key events
      return
    elsif is_lt?(keycode)
      layer_index = lt_layer(keycode)
      tap_keycode = lt_tap_keycode(keycode)
      handle_lt_key(row, col, layer_index, tap_keycode, pressed)
      # LT keys don't generate key events (handled internally)
      return
    elsif is_tg?(keycode)
      layer_index = tg_layer(keycode)
      handle_tg_key(layer_index, pressed)
      # TG keys don't generate key events
      return
    end

    # Update active keys state
    if pressed
      @active_keys[key_pos] = {keycode: keycode, modifier: modifier}
    else
      @active_keys.delete(key_pos)
    end

    # Calculate accumulated modifier from all active keys
    accumulated_modifier = 0
    @active_keys.each do |_, key_info|
      accumulated_modifier |= key_info[:modifier]
    end

    # Prepare keycode for USB HID: 0 on release, actual keycode on press
    keycode_for_hid = pressed ? keycode : 0

    # Send event with accumulated modifier
    @callback&.call(
      row: row,
      col: col,
      keycode: keycode_for_hid,
      modifier: accumulated_modifier,
      pressed: pressed  # Available for debugging/logging
    )
  end

  def resolve_key(row, col)
    index = row * @col_count + col

    # Build layer priority list: locked -> momentary stack (LIFO) -> default
    priority_layers = []

    # 1. Toggle locked layer
    if @locked_layer
      priority_layers << @locked_layer
    end

    # 2. Momentary layers (LIFO - last pressed has highest priority)
    i = @layer_stack.size - 1
    while 0 <= i do
      priority_layers << @layer_stack[i]
      i -= 1
    end

    # 3. Default layer
    if @default_layer
      default_index = @layer_names.index(@default_layer)
      priority_layers << default_index if default_index
    end

    # Search layers in priority order
    priority_layers.each do |layer_index|
      next unless layer_index && layer_index < @layer_names.size

      layer_name = @layer_names[layer_index]
      keymap = @layers[layer_name]
      next unless keymap

      keycode = keymap[index]
      next if keycode == KC_NO  # Transparent - try next layer

      # Found a key - check if it's a modifier
      if is_modifier_key?(keycode)
        # Convert modifier keycode (0xE0-0xE7) to modifier bit (0x01-0x80)
        modifier_bit = 1 << (keycode - 0xE0)
        return [0, modifier_bit]  # Return (keycode=0, modifier=bit)
      else
        return [keycode, 0]  # Return (keycode=keycode, modifier=0)
      end
    end

    # All layers transparent
    return [0, 0]
  end

  def resolve_keycode(row, col)
    keycode, _ = resolve_key(row, col)
    keycode
  end

  def handle_mo_key(row, col, layer_index, pressed)
    # @type var key_pos: [Integer, Integer]
    key_pos = [row, col]

    if pressed
      # Activate momentary layer
      unless @momentary_keys.has_key?(key_pos)
        @layer_stack << layer_index
        @momentary_keys[key_pos] = layer_index
      end
    else
      # Deactivate momentary layer
      if @momentary_keys.has_key?(key_pos)
        deactivate_layer = @momentary_keys[key_pos]
        #@layer_stack.delete(deactivate_layer) # mruby/c does not support Array#delete
        @layer_stack.delete_if{|lyr| lyr == deactivate_layer }
        @momentary_keys.delete(key_pos)
      end
    end
  end

  def handle_tg_key(layer_index, pressed)
    # Only process on key press
    return unless pressed

    # Toggle locked layer
    if @locked_layer == layer_index
      @locked_layer = nil
    else
      @locked_layer = layer_index
    end
  end

  def handle_mt_key(row, col, modifier_index, tap_keycode, pressed)
    # @type var key_pos: [Integer, Integer]
    key_pos = [row, col]

    if pressed
      if state = @tap_hold_keys[key_pos]
        # Re-press: check if within repush threshold for double-tap-hold
        if state[:state] == :tapped
          elapsed = Machine.board_millis - state[:released_at]
          if elapsed < @repush_threshold_ms
            # Double-tap-hold: start repeating tap keycode
            state[:state] = :repeating
            state[:pressed_at] = Machine.board_millis
            # Send initial press event
            @callback&.call(
              row: row,
              col: col,
              keycode: tap_keycode,
              modifier: 0,
              pressed: true
            )
            return
          end
        end
      end

      # Normal press
      @tap_hold_keys[key_pos] = {
        tap_type: :modifier,
        modifier_index: modifier_index,
        tap_keycode: tap_keycode,
        pressed_at: Machine.board_millis,
        state: :pressed,
        other_key_pressed: false
      }
    else
      if state = @tap_hold_keys[key_pos]
        elapsed = Machine.board_millis - state[:pressed_at]

        case state[:state]
        when :pressed
          # Tap action: send tap keycode press and release immediately
          if elapsed < @tap_threshold_ms && !state[:other_key_pressed]
            # Send press event
            @callback&.call(
              row: row,
              col: col,
              keycode: tap_keycode,
              modifier: 0,
              pressed: true
            )
            # Small delay to ensure USB HID processes the press event
            sleep_ms 5
            # Send release event (keycode = 0 for release)
            @callback&.call(
              row: row,
              col: col,
              keycode: 0,
              modifier: 0,
              pressed: false
            )
            # Transition to :tapped state and wait for repush
            state[:state] = :tapped
            state[:released_at] = Machine.board_millis
            return  # Keep in @tap_hold_keys for double-tap detection
          end
        when :holding
          # Deactivate modifier
          deactivate_modifier(key_pos)
        when :repeating
          # Send release event for repeating tap keycode (keycode = 0 for release)
          @callback&.call(
            row: row,
            col: col,
            keycode: 0,
            modifier: 0,
            pressed: false
          )
        end

        # Delete from @tap_hold_keys (unless :tapped waiting for repush)
        @tap_hold_keys.delete(key_pos) unless state[:state] == :tapped
      end
    end
  end

  def handle_lt_key(row, col, layer_index, tap_keycode, pressed)
    # @type var key_pos: [Integer, Integer]
    key_pos = [row, col]

    if pressed
      if state = @tap_hold_keys[key_pos]
        # Re-press: check if within repush threshold for double-tap-hold
        if state[:state] == :tapped
          elapsed = Machine.board_millis - state[:released_at]
          if elapsed < @repush_threshold_ms
            # Double-tap-hold: start repeating tap keycode
            state[:state] = :repeating
            state[:pressed_at] = Machine.board_millis
            # Send initial press event
            @callback&.call(
              row: row,
              col: col,
              keycode: tap_keycode,
              modifier: 0,
              pressed: true
            )
            return
          end
        end
      end

      # Normal press
      @tap_hold_keys[key_pos] = {
        tap_type: :layer,
        layer_index: layer_index,
        tap_keycode: tap_keycode,
        pressed_at: Machine.board_millis,
        state: :pressed,
        other_key_pressed: false
      }
    else
      if state = @tap_hold_keys[key_pos]
        elapsed = Machine.board_millis - state[:pressed_at]

        case state[:state]
        when :pressed
          # Tap action: send tap keycode press and release immediately
          if elapsed < @tap_threshold_ms && !state[:other_key_pressed]
            # Send press event
            @callback&.call(
              row: row,
              col: col,
              keycode: tap_keycode,
              modifier: 0,
              pressed: true
            )
            # Small delay to ensure USB HID processes the press event
            sleep_ms 5
            # Send release event (keycode = 0 for release)
            @callback&.call(
              row: row,
              col: col,
              keycode: 0,
              modifier: 0,
              pressed: false
            )
            # Transition to :tapped state and wait for repush
            state[:state] = :tapped
            state[:released_at] = Machine.board_millis
            return  # Keep in @tap_hold_keys for double-tap detection
          end
        when :holding
          # Deactivate layer
          deactivate_layer(key_pos, layer_index)
        when :repeating
          # Send release event for repeating tap keycode (keycode = 0 for release)
          @callback&.call(
            row: row,
            col: col,
            keycode: 0,
            modifier: 0,
            pressed: false
          )
        end

        # Delete from @tap_hold_keys (unless :tapped waiting for repush)
        @tap_hold_keys.delete(key_pos) unless state[:state] == :tapped
      end
    end
  end

  # Update tap/hold key states (check for timeout and transitions)
  def update_tap_hold_keys
    keys_to_delete = []

    @tap_hold_keys.each do |key_pos, state|
      case state[:state]
      when :pressed
        elapsed = Machine.board_millis - state[:pressed_at]
        if elapsed >= @tap_threshold_ms || state[:other_key_pressed]
          state[:state] = :holding
          # Activate based on tap_type
          if state[:tap_type] == :layer
            activate_layer(key_pos, state[:layer_index])
          elsif state[:tap_type] == :modifier
            activate_modifier(key_pos, state[:modifier_index])
          end
        end

      when :tapped
        # Check if repush threshold has expired
        elapsed = Machine.board_millis - state[:released_at]
        if elapsed >= @repush_threshold_ms
          keys_to_delete << key_pos
        end
      end
    end

    # Safety: If no keys are physically pressed, clean up stuck states
    # (but keep :tapped state which is waiting for double-tap)
    if @keys_pressed.empty? && !@tap_hold_keys.empty?
      @tap_hold_keys.each do |key_pos, state|
        # Keep :tapped state (waiting for double-tap), clean up others
        if state[:state] != :tapped
          keys_to_delete << key_pos
        end
      end
    end

    # Clean up keys marked for deletion
    keys_to_delete.each do |key_pos|
      @tap_hold_keys.delete(key_pos)
    end
  end

  # Mark that another key was pressed (for tap/hold interruption)
  # Immediately activate layer/modifier when another key is pressed
  def mark_other_key_pressed
    @tap_hold_keys.each do |key_pos, state|
      if state[:state] == :pressed
        state[:other_key_pressed] = true
        state[:state] = :holding
        # Activate based on tap_type
        if state[:tap_type] == :layer
          activate_layer(key_pos, state[:layer_index])
        elsif state[:tap_type] == :modifier
          activate_modifier(key_pos, state[:modifier_index])
        end
      end
    end
  end

  def activate_layer(key_pos, layer_index)
    unless @momentary_keys.has_key?(key_pos)
      @layer_stack << layer_index
      @momentary_keys[key_pos] = layer_index
    end
  end

  def deactivate_layer(key_pos, layer_index)
    if @momentary_keys.has_key?(key_pos)
      @layer_stack.delete_if { |lyr| lyr == layer_index }
      @momentary_keys.delete(key_pos)
    end
  end

  def activate_modifier(key_pos, modifier_index)
    modifier_bit = 1 << modifier_index
    @active_keys[key_pos] = {keycode: 0, modifier: modifier_bit}

    # Send modifier state to USB HID
    accumulated_modifier = 0
    @active_keys.each do |_, key_info|
      accumulated_modifier |= key_info[:modifier]
    end

    @callback&.call(
      row: key_pos[0],
      col: key_pos[1],
      keycode: 0,
      modifier: accumulated_modifier,
      pressed: true
    )
  end

  def deactivate_modifier(key_pos)
    @active_keys.delete(key_pos)

    # Send modifier state to USB HID
    accumulated_modifier = 0
    @active_keys.each do |_, key_info|
      accumulated_modifier |= key_info[:modifier]
    end

    @callback&.call(
      row: key_pos[0],
      col: key_pos[1],
      keycode: 0,
      modifier: accumulated_modifier,
      pressed: false
    )
  end
end
