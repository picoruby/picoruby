# Layer switching keycode module
# Defines special keycodes for layer switching functionality
module LayerKeycode
  # Base values for special layer keycodes
  MO_BASE = 0xE000  # Momentary layer switch base
  TG_BASE = 0xE100  # Toggle layer switch base

  # Create a momentary layer switch keycode
  # While the key is pressed, the specified layer becomes active
  # @param layer_index [Integer] Layer index (0-255)
  # @return [Integer] Special keycode for MO(layer_index)
  def self.MO(layer_index)
    raise ArgumentError, "Layer index must be 0-255" unless 0 <= layer_index && layer_index <= 255
    MO_BASE + layer_index
  end

  # Create a toggle layer switch keycode
  # Each press toggles the specified layer on/off
  # @param layer_index [Integer] Layer index (0-255)
  # @return [Integer] Special keycode for TG(layer_index)
  def self.TG(layer_index)
    raise ArgumentError, "Layer index must be 0-255" unless 0 <= layer_index && layer_index <= 255
    TG_BASE + layer_index
  end

  # Check if keycode is a momentary layer switch
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is MO type
  def self.is_mo?(keycode)
    keycode >= MO_BASE && keycode < MO_BASE + 256
  end

  # Check if keycode is a toggle layer switch
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is TG type
  def self.is_tg?(keycode)
    keycode >= TG_BASE && keycode < TG_BASE + 256
  end

  # Extract layer index from MO keycode
  # @param keycode [Integer] MO keycode
  # @return [Integer] Layer index
  def self.mo_layer(keycode)
    keycode - MO_BASE
  end

  # Extract layer index from TG keycode
  # @param keycode [Integer] TG keycode
  # @return [Integer] Layer index
  def self.tg_layer(keycode)
    keycode - TG_BASE
  end
end
