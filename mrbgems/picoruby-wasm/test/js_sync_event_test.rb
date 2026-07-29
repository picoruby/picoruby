# Node has EventTarget / Event as globals, so the synchronous dispatch path can
# be exercised without a DOM. Capture *propagation* cannot: an EventTarget has
# no tree, so only capture-aware removal is asserted here.
class JSSyncEventTest < Picotest::Test
  def new_target
    JS.global[:EventTarget].new
  end

  def new_event(type, cancelable = false)
    options = JS.global.create_object
    options[:cancelable] = cancelable
    JS.global[:Event].new(type, options)
  end

  def test_sync_handler_runs_on_the_dispatch_stack
    target = new_target
    log = []
    id = target.addEventListener('sync-order', sync: true) { |_ev| log << :handler }
    log << :before
    target.dispatchEvent(new_event('sync-order'))
    log << :after
    assert_equal([:before, :handler, :after], log)
    JS::Object.removeEventListener(id)
  end

  def test_async_handler_does_not_run_inline
    target = new_target
    log = []
    id = target.addEventListener('async-order') { |_ev| log << :handler }
    target.dispatchEvent(new_event('async-order'))
    assert_equal([], log)
    JS::Object.removeEventListener(id)
  end

  def test_sync_prevent_default_is_observed_by_dispatch_event
    target = new_target
    id = target.addEventListener('sync-cancel', sync: true) { |ev| ev.preventDefault }
    event = new_event('sync-cancel', true)
    assert_equal(false, target.dispatchEvent(event))
    assert_equal(true, event[:defaultPrevented])
    JS::Object.removeEventListener(id)
  end

  # The reason sync: true exists: an async handler calls preventDefault long
  # after dispatch has finished, so it has no effect.
  def test_async_prevent_default_is_too_late
    target = new_target
    id = target.addEventListener('async-cancel') { |ev| ev.preventDefault }
    event = new_event('async-cancel', true)
    assert_equal(true, target.dispatchEvent(event))
    assert_equal(false, event[:defaultPrevented])
    JS::Object.removeEventListener(id)
  end

  def test_block_receives_js_event
    target = new_target
    seen = nil
    # JS::Object descends from BasicObject, so #is_a? (defined in C) is the
    # way to ask - #class would be forwarded to JS as a property lookup.
    id = target.addEventListener('sync-arg', sync: true) { |ev| seen = ev.is_a?(JS::Event) }
    target.dispatchEvent(new_event('sync-arg'))
    assert_equal(true, seen)
    JS::Object.removeEventListener(id)
  end

  def test_nested_synchronous_dispatch
    outer = new_target
    inner = new_target
    log = []
    inner_id = inner.addEventListener('inner', sync: true) { |_ev| log << :inner }
    outer_id = outer.addEventListener('outer', sync: true) do |_ev|
      log << :outer_begin
      inner.dispatchEvent(new_event('inner'))
      log << :outer_end
    end
    outer.dispatchEvent(new_event('outer'))
    assert_equal([:outer_begin, :inner, :outer_end], log)
    JS::Object.removeEventListener(inner_id)
    JS::Object.removeEventListener(outer_id)
  end

  def test_sync_once_fires_exactly_once
    target = new_target
    count = 0
    target.addEventListener('sync-once', sync: true, once: true) { |_ev| count += 1 }
    target.dispatchEvent(new_event('sync-once'))
    target.dispatchEvent(new_event('sync-once'))
    assert_equal(1, count)
  end

  def test_sync_once_clears_bookkeeping
    target = new_target
    id = target.addEventListener('sync-once-gc', sync: true, once: true) { |_ev| nil }
    target.dispatchEvent(new_event('sync-once-gc'))
    # The DOM dropped the listener, and so did the JS-side registry.
    assert_equal(false, JS::Object.removeEventListener(id))
  end

  def test_passive_suppresses_prevent_default
    target = new_target
    id = target.addEventListener('sync-passive', sync: true, passive: true) do |ev|
      ev.preventDefault
    end
    event = new_event('sync-passive', true)
    assert_equal(true, target.dispatchEvent(event))
    assert_equal(false, event[:defaultPrevented])
    JS::Object.removeEventListener(id)
  end

  def test_capture_listener_is_removable
    target = new_target
    count = 0
    id = target.addEventListener('sync-capture', sync: true, capture: true) { |_ev| count += 1 }
    assert_equal(true, JS::Object.removeEventListener(id))
    target.dispatchEvent(new_event('sync-capture'))
    assert_equal(0, count)
  end

  # An exception must not escape into the JS dispatch frame, and the
  # scheduler_lock taken around the dispatch must be released either way.
  def test_exception_in_sync_handler_is_contained
    target = new_target
    id = target.addEventListener('sync-raise', sync: true) { |_ev| raise 'boom' }
    target.dispatchEvent(new_event('sync-raise'))
    # await goes through mrb_suspend_task, which raises while scheduler_lock is
    # held - so it succeeding proves the lock was released.
    assert_equal(1, JS.eval('Promise.resolve(1)').await)
    JS::Object.removeEventListener(id)
  end

  # Spawning a task is allowed (it is only *suspending* that is not), so a sync
  # handler can register an async listener - which then runs on the scheduler.
  def test_async_listener_can_be_registered_from_sync_handler
    outer = new_target
    inner = new_target
    log = []
    outer_id = outer.addEventListener('register', sync: true) do |_ev|
      inner.addEventListener('registered-later') { |_e| log << :async }
      log << :sync
    end
    outer.dispatchEvent(new_event('register'))
    assert_equal([:sync], log)

    inner.dispatchEvent(new_event('registered-later'))
    assert_equal([:sync], log)
    sleep 0.05
    assert_equal([:sync, :async], log)
    JS::Object.removeEventListener(outer_id)
  end

  # Suspending is impossible on the JS dispatch stack; it has to raise rather
  # than corrupt the scheduler.
  def test_await_inside_sync_handler_raises
    target = new_target
    error = nil
    id = target.addEventListener('sync-await', sync: true) do |_ev|
      begin
        JS.eval('Promise.resolve(1)').await
      rescue => e
        error = e.class
      end
    end
    target.dispatchEvent(new_event('sync-await'))
    assert_equal(RuntimeError, error)
    JS::Object.removeEventListener(id)
  end
end
