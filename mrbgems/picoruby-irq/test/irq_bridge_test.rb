class IRQBridgeTest < Picotest::Test
  # The last slot belongs to no driver, so these tests never collide
  # with a real peripheral. A method rather than a constant: the class
  # body is evaluated before IRQ is reachable from this scope.
  def spare_source
    IRQ::MAX_SOURCES - 1
  end

  def setup
    @q = Task::Queue.new
    # A previous test may have left bits or a token behind; take clears
    # both, which is also what makes the following unbind legal.
    IRQ.take(spare_source)
    IRQ.unbind(spare_source)
    IRQ.bind(spare_source, @q)
  end

  def teardown
    IRQ.take(spare_source)
    IRQ.unbind(spare_source)
  end

  def test_signal_delivers_one_token_carrying_the_ored_bits
    i = 0
    while i < 3
      IRQ.simulate(spare_source, 1 << i)
      i += 1
    end
    Task.pass
    assert_equal 1, @q.size
    assert_equal spare_source, @q.pop
    assert_equal 7, IRQ.take(spare_source)
  end

  def test_no_second_token_while_one_is_outstanding
    IRQ.simulate(spare_source, 1)
    Task.pass
    IRQ.simulate(spare_source, 2)
    Task.pass
    assert_equal 1, @q.size
  end

  def test_a_new_token_follows_a_take
    IRQ.simulate(spare_source, 1)
    Task.pass
    assert_equal spare_source, @q.pop
    assert_equal 1, IRQ.take(spare_source)

    IRQ.simulate(spare_source, 2)
    Task.pass
    assert_equal spare_source, @q.pop
    assert_equal 2, IRQ.take(spare_source)
  end

  def test_take_on_a_spurious_token_returns_zero
    IRQ.simulate(spare_source, 4)
    Task.pass
    @q.pop
    assert_equal 4, IRQ.take(spare_source)
    assert_equal 0, IRQ.take(spare_source)
  end

  def test_bits_signalled_before_bind_are_delivered_after_bind
    IRQ.unbind(spare_source)
    IRQ.simulate(spare_source, 2)
    Task.pass
    assert_equal 0, @q.size

    IRQ.bind(spare_source, @q)
    Task.pass
    assert_equal spare_source, @q.pop
    assert_equal 2, IRQ.take(spare_source)
  end

  def test_unbind_reports_whether_a_binding_was_removed
    assert_true IRQ.unbind(spare_source)
    assert_false IRQ.unbind(spare_source)
  end

  def test_rebinding_the_same_queue_is_a_no_op_even_with_a_token_out
    IRQ.simulate(spare_source, 1)
    Task.pass
    # object_id, not equal?: FemtoRuby has no equal?, and its == treats
    # two objects of the same class as equal.
    assert_equal @q.object_id, IRQ.bind(spare_source, @q).object_id
  end

  def test_rebind_or_unbind_with_an_undelivered_token_raises
    IRQ.simulate(spare_source, 1)
    Task.pass
    assert_raise(RuntimeError) { IRQ.unbind(spare_source) }
    assert_raise(RuntimeError) { IRQ.bind(spare_source, Task::Queue.new) }
  end

  # A push that cannot be delivered must not leave the source claimed:
  # no token exists, so nothing will ever release it and every later
  # interrupt would be swallowed.
  def test_a_source_survives_a_queue_that_went_away
    @q.close
    IRQ.simulate(spare_source, 1)
    Task.pass
    assert_false IRQ.unbind(spare_source)   # the dead binding was dropped

    q = Task::Queue.new
    IRQ.bind(spare_source, q)
    IRQ.simulate(spare_source, 2)
    Task.pass
    assert_equal spare_source, q.pop
    assert_equal 3, IRQ.take(spare_source)
  end

  # GPIO shares one source for the whole subsystem, and it must be a
  # real slot -- binding it is how a task waits for button presses.
  def test_gpio_source_is_a_bindable_source
    src = IRQ.gpio_source
    assert_true 0 <= src
    assert_true src < IRQ::MAX_SOURCES
    assert_not_equal spare_source, src

    q = Task::Queue.new
    IRQ.bind(src, q)
    IRQ.simulate(src, 1)
    Task.pass
    assert_equal src, q.pop
    assert_equal 1, IRQ.take(src)
    IRQ.unbind(src)
  end

  def test_bind_rejects_anything_that_is_not_a_task_queue
    assert_raise(TypeError) { IRQ.bind(spare_source, []) }
  end

  def test_out_of_range_source_ids_raise
    over = IRQ::MAX_SOURCES
    assert_raise(ArgumentError) { IRQ.bind(over, @q) }
    assert_raise(ArgumentError) { IRQ.bind(-1, @q) }
    assert_raise(ArgumentError) { IRQ.unbind(over) }
    assert_raise(ArgumentError) { IRQ.take(over) }
    assert_raise(ArgumentError) { IRQ.take(-1) }
    assert_raise(ArgumentError) { IRQ.simulate(over, 1) }
  end

  # The hypothesis the whole bridge exists for, in the form both
  # runtimes can express: the signal happens before anything drains it,
  # pop parks this task, and the drain that runs on the scheduler's way
  # to idle is what wakes it again.
  def test_a_parked_pop_is_woken_by_the_drain
    IRQ.simulate(spare_source, 8)
    assert_equal spare_source, @q.pop
    assert_equal 8, IRQ.take(spare_source)
  end

  # The same thing with a second task doing the waiting, which is how a
  # driver would really use it. PicoRuby only: FemtoRuby cannot spawn a
  # task from a block (Task.create takes compiled bytecode).
  def test_a_task_blocked_in_pop_wakes_on_signal
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    q = @q
    seen = nil
    Task.new do
      id = q.pop
      seen = [id, IRQ.take(id)]
    end

    # Let the task reach pop and block there.
    i = 0
    while i < 10
      break if 0 < q.num_waiting
      sleep_ms 1
      i += 1
    end
    assert_equal 1, q.num_waiting
    assert_nil seen

    IRQ.simulate(spare_source, 8)
    i = 0
    while i < 10
      break unless seen.nil?
      sleep_ms 1
      i += 1
    end
    assert_equal [spare_source, 8], seen
  end
end
