# Tests for the methods of picoruby-keyboard that run without the scan
# loop: the LayerKeycode encoders and predicates, layer definition, key
# resolution through the layer stack, and the small helpers. These are
# the methods picoruby-aot compiles, so the same tests run against the
# irep build and the AOT build (the twin test) and must agree.

# As a keymap script does (README), so that KC_A and MO() read the same
# inside a layer block and in an assertion. The runner also loads this
# file on the host CRuby to list the test classes, where USB is not
# defined; the includes are for the target VM only.
if Object.const_defined?(:USB)
  include USB::HID::Keycode
  include LayerKeycode
end

class LayerKeycodeTest < Picotest::Test
  def setup
    # A 2x3 matrix. The pins are not touched on the host.
    @kbd = Keyboard.new([0, 1], [2, 3, 4])
  end

  def test_mo_encodes_and_decodes
    assert_equal 0xE000, @kbd.MO(0)
    assert_equal 0xE0FF, @kbd.MO(255)
    assert_equal 7, @kbd.mo_layer(@kbd.MO(7))
    assert @kbd.is_mo?(0xE000)
    assert @kbd.is_mo?(0xE0FF)
    assert_false @kbd.is_mo?(0xDFFF)
    assert_false @kbd.is_mo?(0xE100)
    assert_raise(ArgumentError) { @kbd.MO(256) }
    assert_raise(ArgumentError) { @kbd.MO(-1) }
  end

  def test_tg_encodes_and_decodes
    assert_equal 0xE100, @kbd.TG(0)
    assert_equal 0xE1FF, @kbd.TG(255)
    assert_equal 3, @kbd.tg_layer(@kbd.TG(3))
    assert @kbd.is_tg?(0xE100)
    assert @kbd.is_tg?(0xE1FF)
    assert_false @kbd.is_tg?(0xE0FF)
    assert_false @kbd.is_tg?(0xE200)
    assert_raise(ArgumentError) { @kbd.TG(256) }
  end

  def test_lt_encodes_and_decodes
    assert_equal 0xE200, @kbd.LT(0, 0)
    assert_equal 0xF1FF, @kbd.LT(15, 255)
    code = @kbd.LT(5, KC_SPACE)
    assert_equal 5, @kbd.lt_layer(code)
    assert_equal KC_SPACE, @kbd.lt_tap_keycode(code)
    assert @kbd.is_lt?(0xE200)
    assert @kbd.is_lt?(0xF1FF)
    assert_false @kbd.is_lt?(0xE1FF)
    assert_false @kbd.is_lt?(0xF200)
    assert_raise(ArgumentError) { @kbd.LT(16, 0) }
    assert_raise(ArgumentError) { @kbd.LT(0, 256) }
    assert_raise(ArgumentError) { @kbd.LT(-1, 0) }
  end

  def test_mt_encodes_and_decodes
    assert_equal 0xF200, @kbd.MT(0xE0, 0)
    assert_equal 0xF9FF, @kbd.MT(0xE7, 255)
    code = @kbd.MT(KC_LSFT, KC_A)
    assert_equal 1, @kbd.mt_modifier_index(code)
    assert_equal KC_A, @kbd.mt_tap_keycode(code)
    assert @kbd.is_mt?(0xF200)
    assert @kbd.is_mt?(0xF9FF)
    assert_false @kbd.is_mt?(0xF1FF)
    assert_false @kbd.is_mt?(0xFA00)
    # The MT range starts where the LT range ends: no code is both.
    assert_false @kbd.is_lt?(code)
    assert_false @kbd.is_mt?(@kbd.LT(15, 255))
    assert_raise(ArgumentError) { @kbd.MT(0xDF, 0) }
    assert_raise(ArgumentError) { @kbd.MT(0xE8, 0) }
    assert_raise(ArgumentError) { @kbd.MT(0xE0, 256) }
  end

  def test_s_encodes_and_decodes
    assert_equal 0xFA00, @kbd.S(0)
    assert_equal 0xFAFF, @kbd.S(255)
    assert_equal KC_1, @kbd.sm_keycode(@kbd.S(KC_1))
    assert @kbd.is_sm?(0xFA00)
    assert @kbd.is_sm?(0xFAFF)
    assert_false @kbd.is_sm?(0xF9FF)
    assert_false @kbd.is_sm?(0xFB00)
    assert_raise(ArgumentError) { @kbd.S(256) }
  end

  def test_mc_encodes_and_decodes
    assert_equal 0xFB00, @kbd.MC(0)
    assert_equal 0xFBFF, @kbd.MC(255)
    assert_equal 9, @kbd.mc_index(@kbd.MC(9))
    assert @kbd.is_mc?(0xFB00)
    assert @kbd.is_mc?(0xFBFF)
    assert_false @kbd.is_mc?(0xFAFF)
    assert_false @kbd.is_mc?(0xFC00)
    assert_raise(ArgumentError) { @kbd.MC(256) }
  end

  def test_v_takes_integer_and_float
    assert_equal(-4, @kbd.V(1))
    assert_equal(-2, @kbd.V(0.5))
    assert_equal(-7, @kbd.V(1.75))
    assert_equal(-40, @kbd.V(10))
    # val <= 0 is the safety guard: a non-void Integer, never nil.
    assert_equal 0, @kbd.V(0)
    assert_equal(-4, @kbd.V(-1))
    assert @kbd.is_v?(-1)
    assert @kbd.is_v?(-4)
    assert @kbd.is_v?(-999)
    assert_false @kbd.is_v?(0)
    assert_false @kbd.is_v?(-1000)
    assert_false @kbd.is_v?(KC_A)
    assert_equal 1.75, @kbd.v_width(-7)
    assert_equal 1.0, @kbd.v_width(-4)
  end

  def test_x_takes_integer_and_float
    assert_equal(-1001, @kbd.X(0.25))
    assert_equal(-1002, @kbd.X(0.5))
    assert_equal(-1004, @kbd.X(1))
    assert_equal 0, @kbd.X(0)
    assert @kbd.is_x?(-1000)
    assert @kbd.is_x?(-1004)
    assert_false @kbd.is_x?(-999)
    assert_false @kbd.is_x?(0)
    assert_equal 0.5, @kbd.x_width(-1002)
    assert_equal 1.0, @kbd.x_width(-1004)
  end
end

class KeyboardLayerTest < Picotest::Test
  def setup
    @kbd = Keyboard.new([0, 1], [2, 3, 4])
  end

  def test_layer_builds_keymap_and_layout
    @kbd.layer(:default) do
      row KC_A, KC_B, V(1), KC_C
      row KC_D, MO(1), KC_1
    end
    assert_equal [:default], @kbd.layer_names
    # The keymap drops the vacancy; the layout keeps it, as rows.
    assert_equal [KC_A, KC_B, KC_C, KC_D, 0xE001, KC_1], @kbd.get_layer(:default)
    assert_equal [[KC_A, KC_B, -4, KC_C], [KC_D, 0xE001, KC_1]], @kbd.layout
  end

  def test_layer_checks_size_name_and_block
    assert_raise(ArgumentError) do
      @kbd.layer(:short) { row KC_A, KC_B }
    end
    assert_raise(ArgumentError) do
      @kbd.layer(:long) { row KC_A, KC_B, KC_C, KC_D, KC_A, KC_B, KC_C }
    end
    assert_raise(ArgumentError) do
      @kbd.layer("default") { row KC_A, KC_B, KC_C, KC_D, KC_A, KC_B }
    end
    assert_raise(ArgumentError) { @kbd.layer(:noblock) }
    assert_equal [], @kbd.layer_names
  end

  def test_second_layer_does_not_extend_layout
    @kbd.layer(:default) { row KC_A, KC_B, KC_C, KC_D, KC_A, KC_B }
    @kbd.layer(:fn) { row KC_1, V(2), KC_NO, KC_NO, KC_NO, KC_NO, KC_NO }
    assert_equal [:default, :fn], @kbd.layer_names
    assert_equal 1, @kbd.layout.size
    assert_equal 6, @kbd.get_layer(:fn).size
    assert_nil @kbd.get_layer(:none)
  end

  def test_redefining_a_layer_keeps_one_name
    @kbd.layer(:default) { row KC_A, KC_B, KC_C, KC_D, KC_A, KC_B }
    @kbd.layer(:default) { row KC_B, KC_B, KC_C, KC_D, KC_A, KC_B }
    assert_equal [:default], @kbd.layer_names
    assert_equal KC_B, @kbd.get_layer(:default)[0]
  end

  def test_default_layer_must_exist
    @kbd.layer(:default) { row KC_A, KC_B, KC_C, KC_D, KC_A, KC_B }
    @kbd.layer(:fn) { row KC_1, KC_1, KC_1, KC_1, KC_1, KC_1 }
    @kbd.default_layer = :fn
    assert_equal KC_1, @kbd.send(:resolve_keycode, 0, 0)
    assert_raise(ArgumentError) { @kbd.default_layer = :none }
  end

  def test_add_macro_hands_out_mc_keycodes
    assert_equal 0xFB00, @kbd.add_macro("hello")
    assert_equal 0xFB01, @kbd.add_macro("world")
    i = 2
    while i < 256
      @kbd.add_macro("m")
      i += 1
    end
    assert_raise(ArgumentError) { @kbd.add_macro("one too many") }
  end

  def test_timing_setters
    @kbd.tap_threshold_ms = 150
    @kbd.repush_threshold_ms = 300
    @kbd.combo_term_ms = 40
    assert_equal 150, @kbd.instance_variable_get(:@tap_threshold_ms)
    assert_equal 300, @kbd.instance_variable_get(:@repush_threshold_ms)
    assert_equal 40, @kbd.instance_variable_get(:@combo_term_ms)
  end

  def test_inject_event_checks_bounds
    assert_raise(ArgumentError) { @kbd.inject_event(2, 0, true) }
    assert_raise(ArgumentError) { @kbd.inject_event(0, 3, true) }
    assert_raise(ArgumentError) { @kbd.inject_event(-1, 0, true) }
    assert_raise(ArgumentError) { @kbd.inject_event(0, -1, true) }
    @kbd.inject_event(1, 2, true)
    @kbd.inject_event(1, 2, false)
    assert_equal 2, @kbd.instance_variable_get(:@injected_events).size
  end
end

class KeyboardResolveTest < Picotest::Test
  def setup
    @kbd = Keyboard.new([0, 1], [2, 3, 4])
    @kbd.layer(:default) do
      row KC_A, KC_LCTL, S(KC_1)
      row KC_B, KC_RGUI, MO(1)
    end
    @kbd.layer(:fn) do
      row KC_1, KC_NO, KC_NO
      row KC_2, KC_NO, KC_NO
    end
    @kbd.layer(:nav) do
      row KC_Z, KC_NO, KC_NO
      row KC_NO, KC_NO, KC_NO
    end
  end

  def resolve(row, col)
    @kbd.send(:resolve_key, row, col)
  end

  def test_resolve_key_on_the_default_layer
    assert_equal [KC_A, 0], resolve(0, 0)
    assert_equal [KC_B, 0], resolve(1, 0)
    # A modifier keycode becomes a modifier bit with keycode 0.
    assert_equal [0, 0x01], resolve(0, 1)
    assert_equal [0, 0x80], resolve(1, 1)
    # S() sends Left Shift with the base keycode.
    assert_equal [KC_1, 0x02], resolve(0, 2)
    # A layer keycode is handed back as is for the caller to dispatch.
    assert_equal [0xE001, 0], resolve(1, 2)
  end

  def test_resolve_keycode_drops_the_modifier
    assert_equal KC_A, @kbd.send(:resolve_keycode, 0, 0)
    assert_equal 0, @kbd.send(:resolve_keycode, 0, 1)
    assert_equal KC_1, @kbd.send(:resolve_keycode, 0, 2)
  end

  def test_toggle_layer_locks_and_unlocks
    @kbd.send(:handle_tg_key, 1, true)
    assert_equal [KC_1, 0], resolve(0, 0)
    # KC_NO is transparent: the default layer answers.
    assert_equal [0, 0x01], resolve(0, 1)
    assert_equal [KC_2, 0], resolve(1, 0)
    # A release does nothing.
    @kbd.send(:handle_tg_key, 1, false)
    assert_equal [KC_1, 0], resolve(0, 0)
    # A second press unlocks.
    @kbd.send(:handle_tg_key, 1, true)
    assert_equal [KC_A, 0], resolve(0, 0)
    # Toggling another layer replaces the lock.
    @kbd.send(:handle_tg_key, 1, true)
    @kbd.send(:handle_tg_key, 2, true)
    assert_equal [KC_Z, 0], resolve(0, 0)
    assert_equal [KC_B, 0], resolve(1, 0)
  end

  def test_momentary_layers_stack_last_pressed_first
    @kbd.send(:handle_mo_key, 1, 2, 1, true)
    assert_equal [KC_1, 0], resolve(0, 0)
    @kbd.send(:activate_layer, [0, 1], 2)
    assert_equal [KC_Z, 0], resolve(0, 0)
    # :nav is transparent at (1, 0); :fn below it answers.
    assert_equal [KC_2, 0], resolve(1, 0)
    @kbd.send(:deactivate_layer, [0, 1], 2)
    assert_equal [KC_1, 0], resolve(0, 0)
    @kbd.send(:handle_mo_key, 1, 2, 1, false)
    assert_equal [KC_A, 0], resolve(0, 0)
    assert_equal [], @kbd.instance_variable_get(:@layer_stack)
  end

  def test_locked_layer_wins_over_momentary
    @kbd.send(:handle_tg_key, 2, true)
    @kbd.send(:handle_mo_key, 1, 2, 1, true)
    assert_equal [KC_Z, 0], resolve(0, 0)
    assert_equal [KC_2, 0], resolve(1, 0)
  end

  def test_all_layers_transparent_gives_zero
    @kbd.send(:handle_tg_key, 1, true)
    @kbd.layer(:default) { row KC_NO, KC_NO, KC_NO, KC_NO, KC_NO, KC_NO }
    assert_equal [0, 0], resolve(0, 1)
  end

  def test_resolve_keycode_from_layer
    assert_equal KC_1, @kbd.send(:resolve_keycode_from_layer, 0, 0, 1)
    assert_equal KC_A, @kbd.send(:resolve_keycode_from_layer, 0, 0, 0)
    # KC_NO on the named layer is 0 here, with no fall-through.
    assert_equal 0, @kbd.send(:resolve_keycode_from_layer, 0, 1, 1)
    assert_equal 0, @kbd.send(:resolve_keycode_from_layer, 0, 0, 3)
    assert_equal 0, @kbd.send(:resolve_keycode_from_layer, 0, 0, nil)
  end

  def test_combo_reference_layer
    assert_equal KC_A, @kbd.send(:resolve_keycode_for_combo, 0, 0)
    @kbd.combo_reference_layer = :fn
    assert_equal KC_1, @kbd.send(:resolve_keycode_for_combo, 0, 0)
    assert_equal 0, @kbd.send(:resolve_keycode_for_combo, 0, 1)
    @kbd.combo_reference_layer = nil
    assert_equal KC_A, @kbd.send(:resolve_keycode_for_combo, 0, 0)
    assert_raise(ArgumentError) { @kbd.combo_reference_layer = :none }
  end

  def test_mark_other_key_pressed_promotes_a_pressed_tap_hold
    @kbd.instance_variable_get(:@tap_hold_keys)[[1, 2]] = {
      tap_type: :layer, layer_index: 1, tap_keycode: KC_SPACE,
      pressed_at: 0, state: :pressed, other_key_pressed: false
    }
    @kbd.send(:mark_other_key_pressed)
    state = @kbd.instance_variable_get(:@tap_hold_keys)[[1, 2]]
    assert_equal :holding, state[:state]
    assert state[:other_key_pressed]
    assert_equal [KC_1, 0], resolve(0, 0)
  end

  def test_is_modifier_key
    assert_false @kbd.send(:is_modifier_key?, 0xDF)
    assert @kbd.send(:is_modifier_key?, 0xE0)
    assert @kbd.send(:is_modifier_key?, 0xE7)
    assert_false @kbd.send(:is_modifier_key?, 0xE8)
    assert_false @kbd.send(:is_modifier_key?, 0)
  end

  def test_ascii_to_hid
    assert_equal [KC_A, 0], @kbd.send(:ascii_to_hid, "a")
    assert_equal [KC_Z, 0], @kbd.send(:ascii_to_hid, "z")
    assert_equal [KC_A, 0x02], @kbd.send(:ascii_to_hid, "A")
    assert_equal [KC_Z, 0x02], @kbd.send(:ascii_to_hid, "Z")
    assert_equal [KC_1, 0], @kbd.send(:ascii_to_hid, "1")
    assert_equal [KC_1 + 8, 0], @kbd.send(:ascii_to_hid, "9")
    assert_equal [KC_0, 0], @kbd.send(:ascii_to_hid, "0")
    assert_equal [KC_SPACE, 0], @kbd.send(:ascii_to_hid, " ")
    assert_equal [KC_MINUS, 0x02], @kbd.send(:ascii_to_hid, "_")
    assert_equal [KC_SCOLON, 0x02], @kbd.send(:ascii_to_hid, ":")
    assert_equal [KC_1, 0x02], @kbd.send(:ascii_to_hid, "!")
    assert_equal [KC_ENTER, 0], @kbd.send(:ascii_to_hid, "\n")
    assert_equal [KC_TAB, 0], @kbd.send(:ascii_to_hid, "\t")
    assert_nil @kbd.send(:ascii_to_hid, "#")
  end
end
