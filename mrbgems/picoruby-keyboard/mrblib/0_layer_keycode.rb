# Layer switching keycode module
# Defines special keycodes for layer switching functionality
module LayerKeycode
  # Base values for special layer keycodes
  # MO: 0xE000-0xE0FF (256)
  # TG: 0xE100-0xE1FF (256)
  # LT: 0xE200-0xF1FF (4096 = 16 layers * 256 keycodes)
  # MT: 0xF200-0xF9FF (2048 = 8 modifiers * 256 keycodes)
  # SM: 0xFA00-0xFAFF (256 keycodes, always sends Shift+keycode)
  # MC: 0xFB00-0xFBFF (256 macro slots, types a string on press)
  MO_BASE = 0xE000
  TG_BASE = 0xE100
  LT_BASE = 0xE200
  MT_BASE = 0xF200
  SM_BASE = 0xFA00
  MC_BASE = 0xFB00

  # Create a momentary layer switch keycode
  # While the key is pressed, the specified layer becomes active
  # @param layer_index [Integer] Layer index (0-255)
  # @return [Integer] Special keycode for MO(layer_index)
  def MO(layer_index)
    raise ArgumentError, "Layer index must be 0-255" unless 0 <= layer_index && layer_index <= 255
    MO_BASE + layer_index
  end

  # Create a Layer-Tap keycode
  # Tap sends tap_keycode, hold activates layer
  # @param layer_index [Integer] Layer index (0-15)
  # @param tap_keycode [Integer] Keycode to send on tap (0-255)
  # @return [Integer] Special keycode for LT(layer_index, tap_keycode)
  def LT(layer_index, tap_keycode)
    raise ArgumentError, "Layer index must be 0-15" unless 0 <= layer_index && layer_index <= 15
    raise ArgumentError, "Tap keycode must be 0-255" unless 0 <= tap_keycode && tap_keycode <= 255
    LT_BASE + (layer_index << 8) + tap_keycode
  end

  # Create a toggle layer switch keycode
  # Each press toggles the specified layer on/off
  # @param layer_index [Integer] Layer index (0-255)
  # @return [Integer] Special keycode for TG(layer_index)
  def TG(layer_index)
    raise ArgumentError, "Layer index must be 0-255" unless 0 <= layer_index && layer_index <= 255
    TG_BASE + layer_index
  end

  # Create a Modifier-Tap keycode
  # Tap sends tap_keycode, hold activates modifier
  # @param modifier_keycode [Integer] Modifier keycode (0xE0-0xE7: KC_LCTL-KC_RGUI)
  # @param tap_keycode [Integer] Keycode to send on tap (0-255)
  # @return [Integer] Special keycode for MT(modifier_keycode, tap_keycode)
  def MT(modifier_keycode, tap_keycode)
    unless 0xE0 <= modifier_keycode && modifier_keycode <= 0xE7
      raise ArgumentError, "Modifier keycode must be 0xE0-0xE7 (KC_LCTL-KC_RGUI)"
    end
    raise ArgumentError, "Tap keycode must be 0-255" unless 0 <= tap_keycode && tap_keycode <= 255

    modifier_index = modifier_keycode - 0xE0
    MT_BASE + (modifier_index << 8) + tap_keycode
  end

  # Check if keycode is a momentary layer switch
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is MO type
  def is_mo?(keycode)
    MO_BASE <= keycode && keycode < MO_BASE + 256
  end

  # Check if keycode is a Layer-Tap
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is LT type
  def is_lt?(keycode)
    LT_BASE <= keycode && keycode < LT_BASE + 4096
  end

  # Check if keycode is a toggle layer switch
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is TG type
  def is_tg?(keycode)
    TG_BASE <= keycode && keycode < TG_BASE + 256
  end

  # Check if keycode is a Modifier-Tap
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is MT type
  def is_mt?(keycode)
    MT_BASE <= keycode && keycode < MT_BASE + 2048
  end

  # Create a Shift-modified keycode
  # Pressing the key always sends Left Shift + keycode simultaneously
  # @param keycode [Integer] Keycode to shift (0-255)
  # @return [Integer] Special keycode for S(keycode)
  def S(keycode)
    raise ArgumentError, "Keycode must be 0-255" unless 0 <= keycode && keycode <= 255
    SM_BASE + keycode
  end

  # Check if keycode is a Shift-modified keycode
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is SM type
  def is_sm?(keycode)
    SM_BASE <= keycode && keycode < SM_BASE + 256
  end

  # Extract base keycode from SM keycode
  # @param keycode [Integer] SM keycode
  # @return [Integer] Base keycode
  def sm_keycode(keycode)
    keycode - SM_BASE
  end

  # Create a macro keycode
  # Pressing this key types the string registered at macro_index
  # @param macro_index [Integer] Macro slot index (0-255)
  # @return [Integer] Special keycode for MC(macro_index)
  def MC(macro_index)
    raise ArgumentError, "Macro index must be 0-255" unless 0 <= macro_index && macro_index <= 255
    MC_BASE + macro_index
  end

  # Check if keycode is a macro keycode
  # @param keycode [Integer] Keycode to check
  # @return [Boolean] true if keycode is MC type
  def is_mc?(keycode)
    MC_BASE <= keycode && keycode < MC_BASE + 256
  end

  # Extract macro index from MC keycode
  # @param keycode [Integer] MC keycode
  # @return [Integer] Macro slot index
  def mc_index(keycode)
    keycode - MC_BASE
  end

  # Extract layer index from MO keycode
  # @param keycode [Integer] MO keycode
  # @return [Integer] Layer index
  def mo_layer(keycode)
    keycode - MO_BASE
  end

  # Extract layer index from LT keycode
  # @param keycode [Integer] LT keycode
  # @return [Integer] Layer index
  def lt_layer(keycode)
    (keycode - LT_BASE) >> 8
  end

  # Extract tap keycode from LT keycode
  # @param keycode [Integer] LT keycode
  # @return [Integer] Tap keycode
  def lt_tap_keycode(keycode)
    (keycode - LT_BASE) & 0xFF
  end

  # Extract layer index from TG keycode
  # @param keycode [Integer] TG keycode
  # @return [Integer] Layer index
  def tg_layer(keycode)
    keycode - TG_BASE
  end

  # Extract modifier index from MT keycode
  # @param keycode [Integer] MT keycode
  # @return [Integer] Modifier index (0-7)
  def mt_modifier_index(keycode)
    (keycode - MT_BASE) >> 8
  end

  # Extract tap keycode from MT keycode
  # @param keycode [Integer] MT keycode
  # @return [Integer] Tap keycode
  def mt_tap_keycode(keycode)
    (keycode - MT_BASE) & 0xFF
  end
end
