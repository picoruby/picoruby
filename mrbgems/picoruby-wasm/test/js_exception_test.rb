class JSExceptionTest < Picotest::Test
  def setup
    JS.eval('globalThis.picorubyThrowSyncString = function() { throw "sync"; }')
    JS.eval('globalThis.picorubyThrowSyncError = function() { throw new Error("sync error"); }')
    JS.eval('globalThis.picorubyRejectAsyncString = function() { return Promise.reject("async"); }')
    JS.eval('globalThis.picorubyRejectAsyncError = function() { return Promise.reject(new Error("async error")); }')
    JS.eval('globalThis.picorubyThrowSyncNul = function() { throw new Error("a\u0000b"); }')
    JS.eval('globalThis.picorubyRejectAsyncNul = function() { return Promise.reject(new Error("c\u0000d")); }')
  end

  # The message comes back from JS with its byte length, past an embedded NUL.
  def test_sync_error_message_keeps_embedded_nul
    error = rescued_error { JS.global.picorubyThrowSyncNul }

    assert_equal("a\x00b", error.message)
  end

  def test_rejected_promise_message_keeps_embedded_nul
    error = rescued_error { JS.global.picorubyRejectAsyncNul.await }

    assert_equal("c\x00d", error.message)
  end

  def test_rescues_sync_string_throw
    error = rescued_error { JS.global.picorubyThrowSyncString }

    assert_equal(RuntimeError, error.class)
    assert_equal('sync', error.message)
  end

  def test_rescues_sync_error_throw
    error = rescued_error { JS.global.picorubyThrowSyncError }

    assert_equal(RuntimeError, error.class)
    assert_equal('sync error', error.message)
  end

  def test_rescues_rejected_string_promise
    error = rescued_error { JS.global.picorubyRejectAsyncString.await }

    assert_equal(RuntimeError, error.class)
    assert_equal('async', error.message)
  end

  def test_rescues_rejected_error_promise
    error = rescued_error { JS.global.picorubyRejectAsyncError.await }

    assert_equal(RuntimeError, error.class)
    assert_equal('async error', error.message)
  end

  private

  def rescued_error(&block)
    block.call
    raise 'expected exception was not raised'
  rescue => e
    e
  end
end
