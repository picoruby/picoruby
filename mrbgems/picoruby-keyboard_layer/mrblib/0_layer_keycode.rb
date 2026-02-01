# Layer switching keycode module
# Defines special keycodes for layer switching functionality
module LayerKeycode
  # Base values for special layer keycodes
  MO_BASE = 0xE000  # Momentary layer switch base
  TG_BASE = 0xE100  # Toggle layer switch base
  MO_TAP_BASE = 0xE200  # Momentary with tap keycode base

  # Create a momentary layer switch keycode
  # While the key is pressed, the specified layer becomes active
  # @param layer_index [Integer] Layer index (0-255 for simple MO, 0-15 for MO with tap)
  # @param tap_keycode [Integer, nil] Optional keycode to send on tap (0-255)
  # @return [Integer] Special keycode for MO(layer_index) or MO(layer_index, tap_keycode)
  def MO(layer_index, tap_keycode = nil)
    if tap_keycode
      raise ArgumentError, "Layer index must be 0-15" unless 0 <= layer_index && layer_index <= 15
      raise ArgumentError, "Tap keycode must be 0-255" unless 0 <= tap_keycode && tap_keycode <= 255
      MO_TAP_BASE + (layer_index << 8) + tap_keycode
    else
      raise ArgumentError, "Layer index must be 0-255" unless 0 <= layer_index && layer_index <= 255
      MO_BASE + layer_index
    end
  end

  # Create a toggle layer switch keycode
  # Each press toggles the specified layer on/off
  # @param layer_index [Integer] Layer index (0-255)
  # @return [Integer] Special keycode for TG(layer_index)
  def TG(layer_index)
    raise ArgumentError, "Layer index must be 0-255" unless 0 <= layer_index && layer_index <= 255
    TG_BASE + layer_index
  end

  # Check if keycode is a momentary layer switch
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is MO type (simple or tap/hold)
  def is_mo?(keycode)
    (keycode >= MO_BASE && keycode < MO_BASE + 256) ||
    (keycode >= MO_TAP_BASE && keycode < MO_TAP_BASE + 4096)
  end

  # Check if keycode is a toggle layer switch
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is TG type
  def is_tg?(keycode)
    keycode >= TG_BASE && keycode < TG_BASE + 256
  end

  # Extract layer index from MO keycode
  # @param keycode [Integer] MO keycode (simple or tap/hold)
  # @return [Integer] Layer index
  def mo_layer(keycode)
    if keycode >= MO_TAP_BASE
      (keycode - MO_TAP_BASE) >> 8
    else
      keycode - MO_BASE
    end
  end

  # Extract layer index from TG keycode
  # @param keycode [Integer] TG keycode
  # @return [Integer] Layer index
  def tg_layer(keycode)
    keycode - TG_BASE
  end

  # Check if keycode is a momentary layer switch with tap keycode
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is MO with tap type
  def is_mo_tap?(keycode)
    keycode >= MO_TAP_BASE && keycode < MO_TAP_BASE + 4096
  end

  # Extract tap keycode from MO tap keycode
  # @param keycode [Integer] MO tap keycode
  # @return [Integer, nil] Tap keycode or nil if not MO tap
  def mo_tap_keycode(keycode)
    return nil unless is_mo_tap?(keycode)
    (keycode - MO_TAP_BASE) & 0xFF
  end
end
