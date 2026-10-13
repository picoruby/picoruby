# The posix port has no GPIO interrupts, so the C side of a GPIO
# registration is played here the way ports/rp2040/irq.c does it:
# sixteen slots, an id is its slot + 1 and is reused once freed, and an
# edge goes to the first registration found for its pin and that edge.
# What is under test is mrblib/irq.rb, which only talks to these three.
$irq_test_slots = []
$irq_test_events = []

module IRQ
  class << self
    def register_gpio(pin, event_type, _opts)
      i = 0
      while i < 16
        unless $irq_test_slots[i]
          $irq_test_slots[i] = [pin, event_type]
          return i + 1
        end
        i += 1
      end
      raise "Failed to register GPIO IRQ"
    end

    def unregister_gpio(id)
      return false unless $irq_test_slots[id - 1]
      $irq_test_slots[id - 1] = nil
      # Events still queued for the id become id 0, as the port does.
      i = 0
      while i < $irq_test_events.size
        event = $irq_test_events[i]
        event[0] = 0 if event[0] == id
        i += 1
      end
      true
    end

    def peek_event
      event = $irq_test_events.shift
      event ? event : [nil, 0]
    end
  end
end

class IRQGPIORegistrationTest < Picotest::Test
  def pin
    6
  end

  def other_pin
    7
  end

  def setup
    @seen = []
    @instances = []
  end

  def teardown
    @instances.each { |instance| instance.unregister }
    $irq_test_events.clear
  end

  # FemtoRuby blocks cannot reach the locals of a method that has
  # returned, so the log and a tag travel as the capture.
  def listen(gpio_pin, tag, event_type = GPIO::EDGE_FALL)
    gpio = GPIO.new(gpio_pin, GPIO::IN)
    instance = gpio.irq(event_type, capture: [@seen, tag]) do |_peri, _event_type, cap|
      cap[0] << cap[1]
    end
    @instances << instance
    instance
  end

  # An edge on the pin, queued as the rp2040 port would.
  def queue_edge(gpio_pin, edge = GPIO::EDGE_FALL)
    i = 0
    while i < 16
      slot = $irq_test_slots[i]
      if slot && slot[0] == gpio_pin && 0 < (slot[1] & edge)
        $irq_test_events << [i + 1, edge]
        break
      end
      i += 1
    end
  end

  # An edge on the pin, queued and then dispatched.
  def fire_edge(gpio_pin, edge = GPIO::EDGE_FALL)
    queue_edge(gpio_pin, edge)
    IRQ.process(100)
  end

  def handler_count
    IRQ::HANDLER.size
  end

  def test_registering_a_pin_again_hands_it_to_the_newer_registration
    listen(pin, :old)
    listen(pin, :new)
    fire_edge(pin)
    # The earlier registration used to take every event on the pin.
    assert_equal [:new], @seen
  end

  def test_running_the_same_registration_again_does_not_pile_up
    before = handler_count
    listen(pin, :first)
    listen(pin, :second)
    listen(pin, :third)
    assert_equal before + 1, handler_count
  end

  def test_a_replaced_registration_unregisters_to_false_and_leaves_the_new_one
    old = listen(pin, :old)
    listen(pin, :new)
    assert_false old.unregister
    fire_edge(pin)
    assert_equal [:new], @seen
  end

  def test_a_replaced_id_reused_by_another_pin_is_not_touched
    old = listen(pin, :old)
    newer = listen(pin, :new)    # is given old's id
    newer.unregister
    listen(other_pin, :other)    # and now another pin is
    assert_false old.unregister
    fire_edge(other_pin)
    assert_equal [:other], @seen
  end

  def test_other_pins_are_left_alone
    listen(pin, :a)
    listen(other_pin, :b)
    fire_edge(pin)
    fire_edge(other_pin)
    assert_equal [:a, :b], @seen
  end

  def test_each_edge_of_a_pin_keeps_its_own_registration
    listen(pin, :fall, GPIO::EDGE_FALL)
    listen(pin, :rise, GPIO::EDGE_RISE)
    fire_edge(pin, GPIO::EDGE_FALL)
    fire_edge(pin, GPIO::EDGE_RISE)
    assert_equal [:fall, :rise], @seen
  end

  def test_an_overlapping_event_replaces_the_registration
    listen(pin, :fall, GPIO::EDGE_FALL)
    listen(pin, :both, GPIO::EDGE_FALL | GPIO::EDGE_RISE)
    fire_edge(pin, GPIO::EDGE_FALL)
    assert_equal [:both], @seen
  end

  def test_a_partly_overlapping_registration_is_replaced_whole
    listen(pin, :both, GPIO::EDGE_FALL | GPIO::EDGE_RISE)
    listen(pin, :fall, GPIO::EDGE_FALL)
    fire_edge(pin, GPIO::EDGE_FALL)
    fire_edge(pin, GPIO::EDGE_RISE)
    # Nothing is left on EDGE_RISE.
    assert_equal [:fall], @seen
  end

  def test_an_event_queued_for_a_replaced_registration_is_dropped
    listen(pin, :old, GPIO::EDGE_FALL | GPIO::EDGE_RISE)
    queue_edge(pin, GPIO::EDGE_RISE)
    listen(pin, :new, GPIO::EDGE_FALL)    # is given old's id
    IRQ.process(100)
    assert_equal [], @seen
  end

  def test_unregistering_twice_is_false_the_second_time
    instance = listen(pin, :x)
    assert_true instance.unregister
    assert_false instance.unregister
  end
end
