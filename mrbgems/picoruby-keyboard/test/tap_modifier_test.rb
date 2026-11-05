# Tests that LT/MT tap and combo events keep the modifiers held by other keys.
# They drive handle_event and update_tap_hold_keys through send, so they
# are skipped on FemtoRuby. See keyboard_test.rb for why the keycodes are
# read through KC.
KC = USB::HID::Keycode if Object.const_defined?(:USB)

class KeyboardTapModifierTest < Picotest::Test
  def setup
    # handle_event and update_tap_hold_keys are reached with send.
    skip "FemtoRuby's send cannot reach Ruby-defined methods" if femtoruby?
    @kbd = Keyboard.new([0, 1], [2, 3, 4])
    @kbd.layer(:default) do
      row MT(KC::KC_RSFT, KC::KC_SPACE), MT(KC::KC_LCTL, KC::KC_A), LT(1, KC::KC_B)
      row KC::KC_C, KC::KC_D, KC::KC_NO
    end
    @kbd.layer(:fn) do
      row KC::KC_NO, KC::KC_NO, KC::KC_NO
      row KC::KC_NO, KC::KC_NO, KC::KC_NO
    end
    # Send the deferred tap release and forget the tapped state at once.
    @kbd.instance_variable_set(:@tap_release_ms, 0)
    @kbd.instance_variable_set(:@repush_threshold_ms, 0)
    @events = []
    events = @events
    @kbd.instance_variable_set(:@callback, Proc.new { |e| events << e })
  end

  def press(row, col)
    @kbd.send(:handle_event, {row: row, col: col, pressed: true})
  end

  def release(row, col)
    @kbd.send(:handle_event, {row: row, col: col, pressed: false})
  end

  def tick
    @kbd.send(:update_tap_hold_keys)
  end

  def tap(row, col)
    @events.clear
    press(row, col)
    release(row, col)
    tick  # deferred tap release
    tick  # tapped state expires
    @events.map { |e| [e[:keycode], e[:modifier]] }
  end

  RSFT = 0x20

  def test_mt_tap_keeps_a_held_mt_modifier
    press(0, 0)  # hold SPC/RSFT
    # Shift is reported when the other key goes down, then stays on
    # through every tap instead of being cleared by the first one.
    assert_equal [[0, RSFT], [KC::KC_A, RSFT], [0, RSFT]], tap(0, 1)
    assert_equal [[KC::KC_A, RSFT], [0, RSFT]], tap(0, 1)
    assert_equal [[KC::KC_A, RSFT], [0, RSFT]], tap(0, 1)
  end

  def test_lt_tap_keeps_a_held_mt_modifier
    press(0, 0)
    tap(0, 1)
    assert_equal [[KC::KC_B, RSFT], [0, RSFT]], tap(0, 2)
  end

  def test_tap_without_held_modifier_sends_none
    assert_equal [[KC::KC_A, 0], [0, 0]], tap(0, 1)
  end

  def test_releasing_the_mt_key_clears_its_modifier
    press(0, 0)
    tap(0, 1)
    @events.clear
    release(0, 0)
    assert_equal [[0, 0]], @events.map { |e| [e[:keycode], e[:modifier]] }
    assert_equal [[KC::KC_C, 0], [0, 0]], tap(1, 0)
  end

  def test_combo_key_tap_keeps_a_held_mt_modifier
    @kbd.add_combo([KC::KC_C, KC::KC_D], KC::KC_E)
    press(0, 0)
    tap(0, 1)
    # C is buffered for the combo and sent as a tap on release.
    assert_equal [[KC::KC_C, RSFT], [0, RSFT]], tap(1, 0)
  end

  def test_combo_action_keeps_a_held_mt_modifier
    @kbd.add_combo([KC::KC_C, KC::KC_D], KC::KC_E)
    press(0, 0)
    tap(0, 1)
    @events.clear
    press(1, 0)
    press(1, 1)
    assert_equal [[KC::KC_E, RSFT], [0, RSFT]], @events.map { |e| [e[:keycode], e[:modifier]] }
  end
end
