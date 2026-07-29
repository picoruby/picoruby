# Generic callbacks (register_callback, and blocks handed to a JS method) are
# called synchronously from JavaScript and their return value goes back to JS.
class JSGenericCallbackTest < Picotest::Test
  def setup
    JS.eval('globalThis.picorubyCallWith = function(value, fn) { return fn(value); }')
    JS.eval('globalThis.picorubyCallWith2 = function(a, b, fn) { return fn(a, b); }')
  end

  def test_register_callback_returns_value_to_js
    JS::Object.register_callback('picotestDouble') { |value| value.to_i * 2 }
    assert_equal(42, JS.eval("globalThis.picorubyGenericCallbacks['picotestDouble'](21)"))
  end

  def test_block_passed_to_js_method_runs_inline
    log = []
    log << :before
    result = JS.global.picorubyCallWith(21) do |value|
      log << :handler
      value.to_i + 1
    end
    log << :after
    assert_equal(22, result)
    assert_equal([:before, :handler, :after], log)
  end

  def test_block_receives_every_argument
    assert_equal('3-4', JS.global.picorubyCallWith2(3, 4) { |a, b| "#{a.to_i}-#{b.to_i}" })
  end

  # Regression: the dispatch used to build a Proc from an irep it then freed,
  # leaving a GC-managed Proc pointing at released memory.
  def test_repeated_dispatch_survives_gc
    JS::Object.register_callback('picotestCounted') { |value| value.to_i + 1 }
    100.times do
      JS.eval("globalThis.picorubyGenericCallbacks['picotestCounted'](1)")
    end
    GC.start
    assert_equal(2, JS.eval("globalThis.picorubyGenericCallbacks['picotestCounted'](1)"))
  end

  def test_exception_in_callback_is_contained
    JS::Object.register_callback('picotestRaises') { |_value| raise 'boom' }
    assert_nil(JS.eval("globalThis.picorubyGenericCallbacks['picotestRaises'](1)"))
    # await goes through mrb_suspend_task, which raises while scheduler_lock is
    # held - so it succeeding proves the lock was released.
    assert_equal(1, JS.eval('Promise.resolve(1)').await)
  end

  # No task to suspend on the JS stack: it has to raise, not corrupt the scheduler.
  def test_await_inside_callback_raises
    error = nil
    JS::Object.register_callback('picotestAwaits') do |_value|
      begin
        JS.eval('Promise.resolve(1)').await
      rescue => e
        error = e.class
      end
      nil
    end
    JS.eval("globalThis.picorubyGenericCallbacks['picotestAwaits'](1)")
    assert_equal(RuntimeError, error)
  end
end
