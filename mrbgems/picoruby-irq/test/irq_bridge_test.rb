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
  def test_the_gpio_source_constant_is_bindable
    src = IRQ::SOURCE::GPIO
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

  # --- The dispatcher machinery under IRQ.start ---
  # PicoRuby only: the dispatcher is a task created from a block.
  # on/off are private (internal API); the tests reach them with send
  # because the machinery contracts -- stale tokens, retirement, lock
  # ordering -- are best pinned down at this layer.

  def irq_on(source, &handler)
    IRQ.send(:on, source, &handler)
  end

  def irq_off(source)
    # Runs in ensure blocks even after a skip, so it must be harmless
    # on FemtoRuby -- whose send cannot reach Ruby-defined methods.
    return false if femtoruby?
    IRQ.send(:off, source)
  end

  # Spin the scheduler until the condition holds; the dispatcher is
  # another task, so every delivery needs it to run.
  def wait_for(limit_ms = 100)
    i = 0
    while i < limit_ms
      return true if yield
      sleep_ms 1
      i += 1
    end
    false
  end

  def release_spare_for_on
    IRQ.take(spare_source)
    IRQ.unbind(spare_source)
  end

  def test_start_is_picoruby_only
    skip "the guard fires only on FemtoRuby" unless femtoruby?
    assert_raise(NotImplementedError) { IRQ.start }
    # stop must stay callable from ensure blocks on either VM.
    assert_false IRQ.stop
  end

  # --- IRQ.start / IRQ.stop, the user-facing switch ---

  # The delivery protocol only needs a peripheral that exposes an
  # event source; the spare slot plays one. That this works also
  # proves the protocol is open to any gem, not just UART.
  def make_fake_serial(source)
    klass = Class.new do
      include IRQ
      attr_accessor :event_source_id_value
      def event_source_id
        @event_source_id_value
      end
    end
    fake = klass.new
    fake.event_source_id_value = source
    fake
  end

  def test_start_and_stop_are_idempotent_and_stop_retires
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    wait_for { dispatcher_tasks.empty? }
    assert_false IRQ.stop
    assert_true IRQ.start
    assert_false IRQ.start          # a second start is a no-op
    assert_equal 1, dispatcher_tasks.size
    assert_true IRQ.stop
    assert_false IRQ.stop           # a second stop is a no-op
    assert_true wait_for { dispatcher_tasks.empty? }
  ensure
    IRQ.stop
  end

  def test_start_delivers_peripheral_events
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    fake = make_fake_serial(spare_source)
    seen = []
    instance = fake.irq(1, capture: "tag") do |peri, event_type, capture|
      seen << [peri.equal?(fake), event_type, capture]
    end
    IRQ.start
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { !seen.empty? }
    assert_equal [[true, 1, "tag"]], seen
    instance.unregister
  ensure
    IRQ.stop
  end

  def test_events_signalled_before_start_arrive_after_start
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    fake = make_fake_serial(spare_source)
    seen = []
    instance = fake.irq(1) { |peri, event_type, capture| seen << event_type }
    IRQ.simulate(spare_source, 1)   # nothing is started yet
    Task.pass
    assert_true seen.empty?
    IRQ.start                       # late binding: the bits were latched
    assert_true wait_for { seen == [1] }
    instance.unregister
  ensure
    IRQ.stop
  end

  def test_registration_after_start_is_live_immediately
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    IRQ.start
    fake = make_fake_serial(spare_source)
    seen = []
    instance = fake.irq(1) { |peri, event_type, capture| seen << event_type }
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { seen == [1] }
    instance.unregister
  ensure
    IRQ.stop
  end

  def test_each_instance_gets_only_its_masked_events
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    fake = make_fake_serial(spare_source)
    ones = []
    twos = []
    i1 = fake.irq(1) { |peri, event_type, capture| ones << event_type }
    i2 = fake.irq(2) { |peri, event_type, capture| twos << event_type }
    IRQ.start
    IRQ.simulate(spare_source, 3)
    assert_true wait_for { !ones.empty? && !twos.empty? }
    assert_equal [1], ones
    assert_equal [2], twos
    i1.unregister
    i2.unregister
  ensure
    IRQ.stop
  end

  def test_a_raising_handler_does_not_rob_its_neighbors
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    fake = make_fake_serial(spare_source)
    seen = []
    bad = fake.irq(1) { |peri, event_type, capture| raise "deliberate failure" }
    good = fake.irq(1) { |peri, event_type, capture| seen << event_type }
    IRQ.start
    IRQ.simulate(spare_source, 1)
    # Isolation is per handler: the raiser is reported, its neighbor
    # still gets the bits.
    assert_true wait_for { seen == [1] }
    bad.unregister
    good.unregister
  ensure
    IRQ.stop
  end

  def test_a_handler_that_unregisters_itself_does_not_skip_its_neighbor
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    fake = make_fake_serial(spare_source)
    seen = []
    # @type var first: IRQ::IRQInstance?
    first = nil
    first = fake.irq(1) { |peri, event_type, capture| first.unregister }
    second = fake.irq(1) { |peri, event_type, capture| seen << event_type }
    IRQ.start
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { seen == [1] }
    second.unregister
  ensure
    IRQ.stop
  end

  def test_a_disabled_instance_is_skipped
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    fake = make_fake_serial(spare_source)
    seen = []
    instance = fake.irq(1) { |peri, event_type, capture| seen << event_type }
    IRQ.start
    instance.disable
    IRQ.simulate(spare_source, 1)
    sleep_ms 30                     # long enough for a wrong delivery
    assert_true seen.empty?
    instance.enable
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { seen == [1] }
    instance.unregister
  ensure
    IRQ.stop
  end

  def test_unregister_releases_the_source_for_the_queue_level
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    fake = make_fake_serial(spare_source)
    instance = fake.irq(1) { |peri, event_type, capture| }
    IRQ.start
    instance.unregister
    q = Task::Queue.new
    IRQ.bind(spare_source, q)       # free again: delivery would refuse
    IRQ.simulate(spare_source, 4)
    Task.pass
    assert_equal spare_source, q.pop(timeout_ms: 200)
    assert_equal 4, IRQ.take(spare_source)
    IRQ.unbind(spare_source)
  ensure
    IRQ.stop
  end

  def test_start_refuses_a_source_bound_by_hand_and_rolls_back
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    fake = make_fake_serial(spare_source)
    instance = fake.irq(1) { |peri, event_type, capture| }
    q = Task::Queue.new
    IRQ.bind(spare_source, q)
    assert_raise(ArgumentError) { IRQ.start }
    # The failed start rolled itself back: once the manual binding is
    # gone, a retry works and the registration was kept.
    IRQ.unbind(spare_source)
    seen = []
    instance.unregister
    instance = fake.irq(1) { |peri, event_type, capture| seen << event_type }
    assert_true IRQ.start
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { seen == [1] }
    instance.unregister
  ensure
    IRQ.stop
  end

  def test_on_delivers_bits_to_the_handler
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    seen = []
    irq_on(spare_source) { |bits| seen << bits }
    IRQ.simulate(spare_source, 5)
    assert_true wait_for { !seen.empty? }
    assert_equal [5], seen
  ensure
    irq_off(spare_source)
  end

  def test_a_raising_handler_does_not_stop_the_dispatcher
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    calls = 0
    irq_on(spare_source) do |bits|
      calls += 1
      raise "deliberate failure from the test handler" if calls == 1
    end
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { calls == 1 }
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { calls == 2 }
  ensure
    irq_off(spare_source)
  end

  def test_off_releases_the_source_completely
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    irq_on(spare_source) { |bits| }
    IRQ.simulate(spare_source, 1)
    wait_for { true }
    assert_true irq_off(spare_source)
    assert_false irq_off(spare_source)
    # The source is unbound and holds no token, so the queue-level API
    # can pick it up from scratch.
    q = Task::Queue.new
    IRQ.bind(spare_source, q)
    IRQ.simulate(spare_source, 2)
    Task.pass
    assert_equal spare_source, q.pop
    assert_equal 2, IRQ.take(spare_source)
    IRQ.unbind(spare_source)
  end

  def test_on_refuses_a_source_bound_by_hand
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    # setup bound spare_source to @q, so IRQ.on must not steal it.
    assert_raise(ArgumentError) { irq_on(spare_source) { |b| } }
  end

  # A failed on must leave no handler behind: the C side rejects the
  # source only after the handler is registered, and a dead entry
  # would keep handlers.empty? false forever -- the dispatcher would
  # never retire and the VM could never finish.
  def test_a_failed_on_leaves_no_handler_behind
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    assert_raise(ArgumentError) { irq_on(IRQ::MAX_SOURCES) { |b| } }
    seen = []
    irq_on(spare_source) { |bits| seen << bits }
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { seen == [1] }
    irq_off(spare_source)
    assert_true wait_for { dispatcher_tasks.empty? }
  end

  # The public IRQ.bind rebinds a source silently whenever no token is
  # outstanding, so a binding made after on can replace the
  # dispatcher's. off must then release only its own bookkeeping -- an
  # unconditional take-and-unbind would tear down the newcomer's
  # binding and discard its events.
  def test_off_leaves_a_binding_made_after_on_alone
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    irq_on(spare_source) { |bits| }
    q = Task::Queue.new
    IRQ.bind(spare_source, q)
    assert_true irq_off(spare_source)        # its handler is removed...
    IRQ.simulate(spare_source, 2)
    Task.pass
    assert_equal spare_source, q.pop(timeout_ms: 200)
    assert_equal 2, IRQ.take(spare_source)   # ...but the binding survived
    IRQ.unbind(spare_source)
  end

  def test_a_second_handler_for_the_same_source_raises
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    irq_on(spare_source) { |bits| }
    assert_raise(ArgumentError) { irq_on(spare_source) { |b| } }
  ensure
    irq_off(spare_source)
  end

  def test_on_works_again_after_all_handlers_were_removed
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    # First round: deliver, then off removes the last handler.
    seen = []
    irq_on(spare_source) { |bits| seen << bits }
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { seen.size == 1 }
    irq_off(spare_source)
    # Second round: the resident dispatcher must deliver again.
    irq_on(spare_source) { |bits| seen << bits }
    IRQ.simulate(spare_source, 2)
    assert_true wait_for { seen.size == 2 }
    assert_equal [1, 2], seen
  ensure
    irq_off(spare_source)
  end

  # A token already in the dispatcher's queue outlives IRQ.off. The
  # dispatcher must not keep the bits it takes through such a token:
  # they may have arrived for whoever bound the source afterwards.
  def test_a_stale_token_does_not_steal_bits_from_the_next_consumer
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    entered = 0
    irq_on(spare_source) do |bits|
      entered += 1
      sleep_ms 30
    end
    IRQ.simulate(spare_source, 1)
    assert_true wait_for { entered == 1 } # dispatcher is inside the handler
    IRQ.simulate(spare_source, 2)         # queues a second token behind it
    Task.pass
    irq_off(spare_source)                 # leaves that token stale
    q = Task::Queue.new
    IRQ.bind(spare_source, q)
    IRQ.simulate(spare_source, 4)         # bits for the new consumer
    # Let the dispatcher wake and pop the stale token BEFORE the new
    # consumer looks -- that is the order in which stealing is possible
    # at all. It must give the bits back; a spurious token afterwards is
    # within the contract, lost bits are not.
    sleep_ms 60
    got = 0
    tries = 0
    while got == 0 && tries < 10
      src = q.pop(timeout_ms: 200)
      break if src.nil?
      got = IRQ.take(spare_source)
      tries += 1
    end
    assert_equal 4, got
    # The give-back may have produced one more (spurious) token; release
    # it so unbind is legal.
    IRQ.take(spare_source)
    IRQ.unbind(spare_source)
  end

  # Live dispatchers only: one that already retired (DORMANT) is done,
  # merely not yet reaped by the next IRQ.on.
  def dispatcher_tasks
    # @type var found: Array[Task]
    found = []
    list = Task.list
    i = 0
    while i < list.size
      t = list[i]
      found << t if t.name == "irq_dispatcher" && t.status != :DORMANT
      i += 1
    end
    found
  end

  # The off that removes the last handler must let the dispatcher end:
  # the scheduler only finishes when no task is left waiting, so a
  # dispatcher parked in pop forever would keep the VM (and this very
  # test process) alive after the program is done with interrupts.
  def test_the_dispatcher_retires_after_the_last_off
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    irq_on(spare_source) { |bits| }
    assert_equal 1, dispatcher_tasks.size
    irq_off(spare_source)
    assert_true wait_for { dispatcher_tasks.empty? }
  end

  # Within one on/off burst the dispatcher is reused, not torn down and
  # recreated per cycle. Task.new roots every task it creates, so a
  # dispatcher per cycle would be a leak -- and these cycles
  # deliberately never yield, so the retire requests they queue are all
  # re-checked and discarded by the one dispatcher once it runs.
  def test_on_off_cycles_reuse_one_dispatcher
    skip "FemtoRuby cannot spawn a task from a block" if femtoruby?
    release_spare_for_on
    # A dispatcher from an earlier test may still be on its way out;
    # let it finish so the count below is this burst's alone.
    wait_for { dispatcher_tasks.empty? }
    n = 0
    while n < 5
      irq_on(spare_source) { |bits| }
      irq_off(spare_source)
      n += 1
    end
    assert_true dispatcher_tasks.size <= 1
    # And the survivor still delivers.
    seen = []
    irq_on(spare_source) { |bits| seen << bits }
    IRQ.simulate(spare_source, 3)
    assert_true wait_for { !seen.empty? }
    assert_equal [3], seen
  ensure
    irq_off(spare_source)
  end

  # The parked-pop hypothesis again, with a second task doing the
  # waiting, which is how a driver would really use it. PicoRuby only:
  # FemtoRuby cannot spawn a task from a block (Task.create takes
  # compiled bytecode).
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
