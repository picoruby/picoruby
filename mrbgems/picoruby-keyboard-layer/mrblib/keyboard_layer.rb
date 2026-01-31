# KeyboardLayer class provides layer switching functionality
# It wraps KeyboardMatrix and manages multiple layers (keymaps)
class KeyboardLayer
  KC_NO = 0x00  # Transparent key - fallthrough to lower layer

  # Initialize keyboard layer manager
  # @param row_pins [Array<Integer>] GPIO pin numbers for rows
  # @param col_pins [Array<Integer>] GPIO pin numbers for columns
  # @param debounce_time [Integer] Debounce time in milliseconds (default: 5)
  def initialize(row_pins, col_pins, debounce_time: 5)
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

    # Create empty keymap for KeyboardMatrix
    # We only use row/col from events, not the keycode
    empty_keymap = Array.new(@row_count * @col_count, KC_NO)

    # Initialize underlying KeyboardMatrix
    @matrix = KeyboardMatrix.new(row_pins, col_pins, empty_keymap)
    @matrix.debounce_time = debounce_time

    # User callback for key events
    @callback = nil
  end

  # Add a layer with name and keymap
  # @param name [Symbol] Layer name
  # @param keymap [Array<Integer>] Keymap array (row_count * col_count)
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

  # Set default layer
  # @param name [Symbol] Layer name
  def default_layer=(name)
    unless @layers.has_key?(name)
      raise ArgumentError, "Layer :#{name} does not exist"
    end
    @default_layer = name
  end

  # Set tap threshold in milliseconds
  # @param value [Integer] Tap threshold in milliseconds
  def tap_threshold_ms=(value)
    @tap_threshold_ms = value
  end

  # Set event callback
  # @param block [Proc] Callback block that receives event hash
  def on_key_event(&block)
    @callback = block
  end

  # Start scanning loop
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

  # Handle key event from KeyboardMatrix
  # @param event [Hash] Event from KeyboardMatrix with :row, :col, :pressed
  def handle_event(event)
    row = event[:row]
    col = event[:col]
    pressed = event[:pressed]

    # Resolve keycode from layers
    unless keycode = resolve_keycode(row, col)
      # No keycode found - ignore
      return
    end

    # Handle special layer keycodes
    if LayerKeycode.is_mo?(keycode)
      layer_index = LayerKeycode.mo_layer(keycode)

      if LayerKeycode.is_mo_tap?(keycode)
        if tap_keycode = LayerKeycode.mo_tap_keycode(keycode)
          handle_mo_tap_key(row, col, layer_index, tap_keycode, pressed)
        end
      else
        handle_mo_key(row, col, layer_index, pressed)
      end
      # MO keys don't generate key events
      return
    elsif LayerKeycode.is_tg?(keycode)
      layer_index = LayerKeycode.tg_layer(keycode)
      handle_tg_key(layer_index, pressed)
      # TG keys don't generate key events
      return
    end

    # Track other key presses for MO tap interruption
    if pressed && keycode != KC_NO
      mark_other_key_pressed
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

  # Resolve keycode from layer stack
  # @param row [Integer] Row index
  # @param col [Integer] Column index
  # @return [Integer, nil] Resolved keycode or nil
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

  # Handle momentary layer key press/release
  # @param row [Integer] Row index
  # @param col [Integer] Column index
  # @param layer_index [Integer] Layer index to activate
  # @param pressed [Boolean] true if pressed, false if released
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

  # Handle toggle layer key press
  # @param layer_index [Integer] Layer index to toggle
  # @param pressed [Boolean] true if pressed, false if released
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

  # Handle MO tap/hold key press/release
  # @param row [Integer] Row index
  # @param col [Integer] Column index
  # @param layer_index [Integer] Layer index to activate on hold
  # @param tap_keycode [Integer] Keycode to send on tap
  # @param pressed [Boolean] true if pressed, false if released
  def handle_mo_tap_key(row, col, layer_index, tap_keycode, pressed)
    # @type var key_pos: [Integer, Integer]
    key_pos = [row, col]

    if pressed
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
          # Tap action: send tap keycode
          if elapsed < @tap_threshold_ms && !mo_state[:other_key_pressed]
            send_tap_keycode(row, col, tap_keycode)
          end
        when :holding
          # Deactivate layer
          deactivate_layer(key_pos, layer_index)
        end

        @mo_tap_keys.delete(key_pos)
      end
    end
  end

  # Update MO tap key states (check for timeout and transitions)
  def update_mo_tap_keys
    @mo_tap_keys.each do |key_pos, mo_state|
      next if mo_state[:state] != :pressed

      elapsed = Machine.board_millis - mo_state[:pressed_at]

      if elapsed >= @tap_threshold_ms || mo_state[:other_key_pressed]
        mo_state[:state] = :holding
        activate_layer(key_pos, mo_state[:layer_index])
      end
    end
  end

  # Mark that another key was pressed (for MO tap interruption)
  def mark_other_key_pressed
    @mo_tap_keys.each do |key_pos, mo_state|
      if mo_state[:state] == :pressed
        mo_state[:other_key_pressed] = true
      end
    end
  end

  # Activate a momentary layer
  # @param key_pos [Array<Integer>] Key position [row, col]
  # @param layer_index [Integer] Layer index to activate
  def activate_layer(key_pos, layer_index)
    unless @momentary_keys.has_key?(key_pos)
      @layer_stack << layer_index
      @momentary_keys[key_pos] = layer_index
    end
  end

  # Deactivate a momentary layer
  # @param key_pos [Array<Integer>] Key position [row, col]
  # @param layer_index [Integer] Layer index to deactivate
  def deactivate_layer(key_pos, layer_index)
    if @momentary_keys.has_key?(key_pos)
      @layer_stack.delete_if { |lyr| lyr == layer_index }
      @momentary_keys.delete(key_pos)
    end
  end

  # Send tap keycode as a quick press/release event
  # @param row [Integer] Row index
  # @param col [Integer] Column index
  # @param keycode [Integer] Keycode to send
  def send_tap_keycode(row, col, keycode)
    # Send press event
    @callback&.call(
      row: row,
      col: col,
      keycode: keycode,
      modifier: 0,
      pressed: true
    )
    # Send release event
    @callback&.call(
      row: row,
      col: col,
      keycode: keycode,
      modifier: 0,
      pressed: false
    )
  end
end
