# The Machine.sleep wrapper matches level: against the GPIO event
# constants, which the irq gem defines by reopening GPIO. This test
# binary carries neither the gpio nor the irq gem, so provide the
# constants (values from picoruby-irq/include/irq.h).
class GPIO
  LEVEL_LOW = 1
  LEVEL_HIGH = 2
  EDGE_FALL = 4
  EDGE_RISE = 8
end

# A wake source for Machine.sleep only needs #pin (duck typing; the
# real GPIO class is not in this test binary). A named class rather
# than Class.new so it works identically on both VMs.
class MachineTestFakePin
  def initialize(pin)
    @pin_value = pin
  end

  def pin
    @pin_value
  end
end

class MachineTest < Picotest::Test
  # The Machine.sleep wrapper uses required keyword args plus **opt.
  # This probe proves the VM supports that shape at runtime -- the
  # mruby/c kwarg opcodes (OP_KARG and friends) exist, but nothing in
  # the tree exercised them with real keywords before this.
  def kw_probe(deep:, source:, **opt)
    [deep, source, opt]
  end

  def test_required_keyword_args_work_on_this_vm
    got = kw_probe(deep: true, source: :timer, ms: 2000)
    assert_equal true, got[0]
    assert_equal :timer, got[1]
    assert_equal 2000, got[2][:ms]
  end

  # --- validation ---------------------------------------------------

  def test_deep_must_be_a_boolean
    assert_raise(TypeError) { Machine.sleep(deep: 1, source: :timer, ms: 10) }
    assert_raise(TypeError) { Machine.sleep(deep: nil, source: :timer, ms: 10) }
  end

  def test_timer_source_requires_ms
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :timer) }
  end

  def test_ms_must_be_an_integer
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :timer, ms: 1.5) }
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :timer, ms: "10") }
  end

  def test_ms_range
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :timer, ms: 0) }
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :timer, ms: -1) }
    # The C side takes uint32_t; 2**32 must be rejected, not wrapped.
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :timer, ms: 4294967296) }
  end

  def test_unknown_options_are_rejected
    # level: is meaningless for a timer wake; silence would hide bugs.
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :timer, ms: 10, level: GPIO::EDGE_FALL) }
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :timer, ms: 10, bogus: 1) }
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: MachineTestFakePin.new(5), level: GPIO::EDGE_FALL, ms: 10) }
  end

  def test_source_must_be_timer_or_pin_like
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: :gpio, ms: 10) }
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: 42, ms: 10) }
  end

  def test_gpio_source_requires_level
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: MachineTestFakePin.new(5)) }
  end

  def test_level_must_be_a_single_gpio_event
    pin = MachineTestFakePin.new(5)
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: pin, level: 3) }
    # A combined mask is not a wake condition the hardware can take.
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: pin, level: 12) }
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: pin, level: nil) }
  end

  def test_pin_must_be_an_integer
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: MachineTestFakePin.new("5"), level: GPIO::EDGE_FALL) }
  end

  def test_pin_range_is_checked_before_narrowing
    # 64-bit Integers: 2**32 would truncate to pin 0 at a careless
    # cast, and a negative would come back as a huge unsigned.
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: MachineTestFakePin.new(4294967296), level: GPIO::EDGE_FALL) }
    assert_raise(ArgumentError) { Machine.sleep(deep: false, source: MachineTestFakePin.new(-1), level: GPIO::EDGE_FALL) }
  end

  # --- behavior on the posix port ----------------------------------

  def test_timer_sleep_returns_nil_and_takes_its_time
    started = Machine.board_millis
    ret = Machine.sleep(deep: false, source: :timer, ms: 30)
    elapsed = Machine.board_millis - started
    assert_nil ret
    # The posix port loops nanosleep over EINTR (the scheduler tick
    # signals every few ms); returning early would mean one EINTR
    # ended the sleep.
    assert 25 <= elapsed
  end

  def test_deep_timer_sleep_works_on_posix_too
    started = Machine.board_millis
    ret = Machine.sleep(deep: true, source: :timer, ms: 30)
    elapsed = Machine.board_millis - started
    assert_nil ret
    assert 25 <= elapsed
  end

  def test_gpio_wake_is_not_available_on_posix
    skip "GPIO wake exists on real hardware" unless Machine.posix?
    assert_raise(NotImplementedError) do
      Machine.sleep(deep: false, source: MachineTestFakePin.new(5), level: GPIO::EDGE_FALL)
    end
  end
end
