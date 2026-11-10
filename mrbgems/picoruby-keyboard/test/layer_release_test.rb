# Tests that a key is released as the key resolved at its press.
# They drive handle_event and update_tap_hold_keys through send, so they
# are skipped on FemtoRuby. See keyboard_test.rb for why the keycodes are
# read through KC.
KC = USB::HID::Keycode if Object.const_defined?(:USB)

class KeyboardLayerReleaseTest < Picotest::Test
  def setup
    # handle_event and update_tap_hold_keys are reached with send.
    skip "FemtoRuby's send cannot reach Ruby-defined methods" if femtoruby?
    @kbd = Keyboard.new([0, 1], [2, 3, 4])
    @kbd.layer(:default) do
      row MT(KC::KC_LCTL, KC::KC_A), LT(1, KC::KC_N), LT(1, KC::KC_B)
      row KC::KC_C, KC::KC_NO, KC::KC_NO
    end
    @kbd.layer(:sym) do
      row KC::KC_LCTL, KC::KC_SLASH, KC::KC_NO
      row KC::KC_LEFT, KC::KC_NO, KC::KC_NO
    end
    # LT/MT keys become hold at the next tick.
    @kbd.instance_variable_set(:@tap_threshold_ms, 0)
    @events = []
    events = @events
    @kbd.instance_variable_set(:@callback, Proc.new { |e| events << [e[:keycode], e[:modifier]] })
  end

  def press(row, col)
    @kbd.send(:handle_event, {row: row, col: col, pressed: true})
    @kbd.send(:update_tap_hold_keys)
  end

  def release(row, col)
    @kbd.send(:handle_event, {row: row, col: col, pressed: false})
    @kbd.send(:update_tap_hold_keys)
  end

  def test_modifier_from_a_layer_is_released_after_the_layer_key
    press(0, 2)    # hold B: :sym
    press(0, 0)    # KC_LCTL on :sym (A_LCTL on :default)
    press(1, 0)    # KC_LEFT with Ctrl
    release(1, 0)
    release(0, 2)  # leave :sym first
    @events.clear
    release(0, 0)
    assert_equal [[0, 0]], @events
    assert_equal({}, @kbd.instance_variable_get(:@active_keys))
  end

  def test_key_from_a_layer_is_released_after_the_layer_key
    press(0, 2)
    press(0, 1)    # KC_SLASH on :sym (LT on :default)
    release(0, 2)
    @events.clear
    release(0, 1)
    assert_equal [[0, 0]], @events
  end

  def test_mt_held_into_a_layer_is_released_as_mt
    press(0, 0)    # A_LCTL becomes Ctrl at the next tick
    press(0, 2)    # hold B: :sym, where (0, 0) is KC_LCTL
    @events.clear
    release(0, 0)
    assert_equal [[0, 0]], @events
    assert_equal({}, @kbd.instance_variable_get(:@tap_hold_keys).select { |k, _| k == [0, 0] })
  end
end
