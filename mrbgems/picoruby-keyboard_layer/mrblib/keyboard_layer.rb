require 'keyboard_matrix'

# KeyboardLayer class provides layer switching functionality
# It wraps KeyboardMatrix and manages multiple layers (keymaps)
class KeyboardLayer
  include LayerKeycode

  KC_NO = 0x00  # Transparent key - fallthrough to lower layer

  def initialize(row_pins, col_pins, debounce_ms: 40)
    @row_count = row_pins.size
    @col_count = col_pins.size

    # Layer management
    @layers = {}                # layer_name => keymap array
    @layer_names = []           # layer_index => layer_name
    @default_layer = nil        # Default layer name
    @locked_layer = nil         # Toggle locked layer index (or nil)
    @layer_stack = []           # Momentary layer stack (LIFO)
    @momentary_keys = {}        # {[row, col] => layer_index} - track MO keys

    # MO tap/hold management
    @mo_tap_keys = {}           # {[row, col] => mo_tap_state}
    @tap_threshold_ms = 200     # Tap threshold in milliseconds
    @repush_threshold_ms = 200  # Repush threshold for double-tap-hold
    @keys_pressed = {}          # Track physically pressed keys: {[row, col] => true}

    # Create empty keymap for KeyboardMatrix
    # We only use row/col from events, not the keycode
    empty_keymap = Array.new(@row_count * @col_count, KC_NO)

    # Initialize underlying KeyboardMatrix
    @matrix = KeyboardMatrix.new(row_pins, col_pins, empty_keymap)
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

  def on_key_event(&block)
    @callback = block
  end

  def start
    unless @callback
      raise "Callback block is required. Use on_key_event to set a callback."
    end

    @matrix.start do |event|
      handle_event(event)
      update_mo_tap_keys
    end
  end

  private

  def handle_event(event)
    row = event[:row]
    col = event[:col]
    pressed = event[:pressed]
    key_pos = [row, col]

    # Track physically pressed keys for safety cleanup
    if pressed
      @keys_pressed[key_pos] = true
    else
      @keys_pressed.delete(key_pos)
    end

    # Track other key presses for MO tap interruption BEFORE resolving keycode
    # This ensures layers are activated immediately when another key is pressed
    if pressed
      mark_other_key_pressed
    end

    # Resolve keycode from layers
    unless keycode = resolve_keycode(row, col)
      # No keycode found - ignore
      return
    end

    # Handle special layer keycodes
    if is_mo?(keycode)
      layer_index = mo_layer(keycode)

      if is_mo_tap?(keycode)
        if tap_keycode = mo_tap_keycode(keycode)
          handle_mo_tap_key(row, col, layer_index, tap_keycode, pressed)
        end
      else
        handle_mo_key(row, col, layer_index, pressed)
      end
      # MO keys don't generate key events
      return
    elsif is_tg?(keycode)
      layer_index = tg_layer(keycode)
      handle_tg_key(layer_index, pressed)
      # TG keys don't generate key events
      return
    end

    # Normal key event - pass to user callback
    if keycode && keycode != KC_NO
      @callback&.call(
        row: row,
        col: col,
        keycode: keycode,
        modifier: event[:modifier] || 0,
        pressed: pressed
      )
    end
  end

  def resolve_keycode(row, col)
    index = row * @col_count + col

    # Build layer priority list: locked -> momentary stack (LIFO) -> default
    priority_layers = []

    # 1. Toggle locked layer
    if @locked_layer
      priority_layers << @locked_layer
    end

    # 2. Momentary layers (LIFO - last pressed has highest priority)
    #@layer_stack.reverse.each do |layer_index| # mruby/c does not support Array#each in reverse
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
      # If not transparent (KC_NO), use this keycode
      return keycode if keycode != KC_NO
    end

    # All layers are transparent - return KC_NO
    KC_NO
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

  def handle_mo_tap_key(row, col, layer_index, tap_keycode, pressed)
    # @type var key_pos: [Integer, Integer]
    key_pos = [row, col]

    if pressed
      if mo_state = @mo_tap_keys[key_pos]
        # Re-press: check if within repush threshold for double-tap-hold
        if mo_state[:state] == :tapped
          elapsed = Machine.board_millis - mo_state[:released_at]
          if elapsed < @repush_threshold_ms
            # Double-tap-hold: start repeating tap keycode
            mo_state[:state] = :repeating
            mo_state[:pressed_at] = Machine.board_millis
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
      @mo_tap_keys[key_pos] = {
        layer_index: layer_index,
        tap_keycode: tap_keycode,
        pressed_at: Machine.board_millis,
        state: :pressed,
        other_key_pressed: false
      }
    else
      if mo_state = @mo_tap_keys[key_pos]
        elapsed = Machine.board_millis - mo_state[:pressed_at]

        case mo_state[:state]
        when :pressed
          # Tap action: send tap keycode once and enter tapped state
          if elapsed < @tap_threshold_ms && !mo_state[:other_key_pressed]
            # Send press event
            @callback&.call(
              row: row,
              col: col,
              keycode: tap_keycode,
              modifier: 0,
              pressed: true
            )
            # Enter tapped_releasing state: release will be sent in next cycle, then wait for repush
            mo_state[:state] = :tapped_releasing
            mo_state[:released_at] = Machine.board_millis
            mo_state[:tap_keycode] = tap_keycode
            return  # Don't delete yet, release will be sent in next cycle
          end
        when :holding
          # Deactivate layer
          deactivate_layer(key_pos, layer_index)
        when :repeating
          # Send release event for repeating tap keycode
          @callback&.call(
            row: row,
            col: col,
            keycode: tap_keycode,
            modifier: 0,
            pressed: false
          )
        end

        @mo_tap_keys.delete(key_pos)
      end
    end
  end

  # Update MO tap key states (check for timeout and transitions)
  def update_mo_tap_keys
    keys_to_delete = []

    @mo_tap_keys.each do |key_pos, mo_state|
      case mo_state[:state]
      when :pressed
        elapsed = Machine.board_millis - mo_state[:pressed_at]

        if elapsed >= @tap_threshold_ms || mo_state[:other_key_pressed]
          mo_state[:state] = :holding
          activate_layer(key_pos, mo_state[:layer_index])
        end
      when :tapped_releasing
        # Send release event for tap action, then transition to tapped state
        row, col = key_pos
        @callback&.call(
          row: row,
          col: col,
          keycode: mo_state[:tap_keycode],
          modifier: 0,
          pressed: false
        )
        # Small delay to ensure USB release event is processed
        sleep_ms(1)
        # Transition to tapped state and wait for possible repush
        mo_state[:state] = :tapped
      when :tapped
        # Check if repush threshold has expired
        elapsed = Machine.board_millis - mo_state[:released_at]
        if elapsed >= @repush_threshold_ms
          keys_to_delete << key_pos
        end
      when :releasing
        # Send release event for tap action
        row, col = key_pos
        @callback&.call(
          row: row,
          col: col,
          keycode: mo_state[:tap_keycode],
          modifier: 0,
          pressed: false
        )
        keys_to_delete << key_pos
      end
    end

    # Safety: If no keys are physically pressed, clean up stuck :repeating states
    if @keys_pressed.empty?
      @mo_tap_keys.each do |key_pos, mo_state|
        # Only clean up :repeating state (not :tapped which is waiting for double-tap)
        if mo_state[:state] == :repeating
          row, col = key_pos
          keycode = mo_state[:tap_keycode]
          @callback&.call(
            row: row,
            col: col,
            keycode: keycode,
            modifier: 0,
            pressed: false
          )
          keys_to_delete << key_pos
        end
      end
    end

    # Clean up released keys
    keys_to_delete.each do |key_pos|
      @mo_tap_keys.delete(key_pos)
    end
  end

  # Mark that another key was pressed (for MO tap interruption)
  # Immediately activate layer when another key is pressed
  def mark_other_key_pressed
    @mo_tap_keys.each do |key_pos, mo_state|
      if mo_state[:state] == :pressed
        mo_state[:other_key_pressed] = true
        mo_state[:state] = :holding
        activate_layer(key_pos, mo_state[:layer_index])
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

  # Send tap keycode as a quick press/release event
  # Schedule release for next scan cycle to ensure proper USB timing
  def send_tap_keycode(row, col, keycode)
    # Send press event
    @callback&.call(
      row: row,
      col: col,
      keycode: keycode,
      modifier: 0,
      pressed: true
    )
    # Schedule release event for next update
    # @type var key_pos: [Integer, Integer]
    key_pos = [row, col]
    @mo_tap_keys[key_pos] = {
      layer_index: nil,
      tap_keycode: keycode,
      pressed_at: Machine.board_millis,
      state: :releasing,
      other_key_pressed: false
    }
  end
end
