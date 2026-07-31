class IRQBridgeTest < Picotest::Test
  # Source 31 belongs to no driver, so these tests never collide with a
  # real peripheral. It also exercises the top bit of the ready mask,
  # where a signed `1 << id` would be undefined behaviour.
  SRC = 31

  def setup
    @q = Task::Queue.new
    # A previous test may have left bits or a token behind; take clears
    # both, which is also what makes the following unbind legal.
    IRQ.take(SRC)
    IRQ.unbind(SRC)
    IRQ.bind(SRC, @q)
  end

  def teardown
    IRQ.take(SRC)
    IRQ.unbind(SRC)
  end

  def test_signal_delivers_one_token_carrying_the_ored_bits
    i = 0
    while i < 3
      IRQ.simulate(SRC, 1 << i)
      i += 1
    end
    Task.pass
    assert_equal 1, @q.size
    assert_equal SRC, @q.pop
    assert_equal 7, IRQ.take(SRC)
  end

  def test_no_second_token_while_one_is_outstanding
    IRQ.simulate(SRC, 1)
    Task.pass
    IRQ.simulate(SRC, 2)
    Task.pass
    assert_equal 1, @q.size
  end

  def test_a_new_token_follows_a_take
    IRQ.simulate(SRC, 1)
    Task.pass
    assert_equal SRC, @q.pop
    assert_equal 1, IRQ.take(SRC)

    IRQ.simulate(SRC, 2)
    Task.pass
    assert_equal SRC, @q.pop
    assert_equal 2, IRQ.take(SRC)
  end

  def test_take_on_a_spurious_token_returns_zero
    IRQ.simulate(SRC, 4)
    Task.pass
    @q.pop
    assert_equal 4, IRQ.take(SRC)
    assert_equal 0, IRQ.take(SRC)
  end

  def test_bits_signalled_before_bind_are_delivered_after_bind
    IRQ.unbind(SRC)
    IRQ.simulate(SRC, 2)
    Task.pass
    assert_equal 0, @q.size

    IRQ.bind(SRC, @q)
    Task.pass
    assert_equal SRC, @q.pop
    assert_equal 2, IRQ.take(SRC)
  end

  def test_unbind_reports_whether_a_binding_was_removed
    assert_true IRQ.unbind(SRC)
    assert_false IRQ.unbind(SRC)
  end

  def test_rebinding_the_same_queue_is_a_no_op_even_with_a_token_out
    IRQ.simulate(SRC, 1)
    Task.pass
    assert_true IRQ.bind(SRC, @q).equal?(@q)
  end

  def test_rebind_or_unbind_with_an_undelivered_token_raises
    IRQ.simulate(SRC, 1)
    Task.pass
    assert_raise(RuntimeError) { IRQ.unbind(SRC) }
    assert_raise(RuntimeError) { IRQ.bind(SRC, Task::Queue.new) }
  end

  # A push that cannot be delivered must not leave the source claimed:
  # no token exists, so nothing will ever release it and every later
  # interrupt would be swallowed.
  def test_a_source_survives_a_queue_that_went_away
    @q.close
    IRQ.simulate(SRC, 1)
    Task.pass
    assert_false IRQ.unbind(SRC)   # the dead binding was dropped

    q = Task::Queue.new
    IRQ.bind(SRC, q)
    IRQ.simulate(SRC, 2)
    Task.pass
    assert_equal SRC, q.pop
    assert_equal 3, IRQ.take(SRC)
  end

  def test_bind_rejects_anything_that_is_not_a_task_queue
    assert_raise(TypeError) { IRQ.bind(SRC, []) }
  end

  def test_out_of_range_source_ids_raise
    assert_raise(ArgumentError) { IRQ.bind(32, @q) }
    assert_raise(ArgumentError) { IRQ.bind(-1, @q) }
    assert_raise(ArgumentError) { IRQ.unbind(32) }
    assert_raise(ArgumentError) { IRQ.take(32) }
    assert_raise(ArgumentError) { IRQ.take(-1) }
    assert_raise(ArgumentError) { IRQ.simulate(32, 1) }
  end

  # The hypothesis the whole bridge exists for: an event raised outside
  # the task can wake a task parked in Task::Queue#pop.
  def test_a_task_blocked_in_pop_wakes_on_signal
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

    IRQ.simulate(SRC, 8)
    i = 0
    while i < 10
      break unless seen.nil?
      sleep_ms 1
      i += 1
    end
    assert_equal [SRC, 8], seen
  end
end
