# Tests for the tagged EM_JS bridge: typed errors, availability-only
# fallback, the bounded onblocked wait, and open-callback cleanup.
#
# The Node test runtime has no real IndexedDB, so these tests install small
# JS fakes (request / transaction / indexedDB objects whose handler setters
# fire asynchronously) and drive the real bridge code through them. The
# fakes only fake the DOM side; the EM_JS promises, the tag unwrapping, and
# the error classification under test are the production code paths.
class IndexedDBBridgeTest < Picotest::Test
  FAKE_JS = <<~JS
    globalThis.__fakeIdb = {
      lateClosed: false,
      lateUpgradeAborted: false,
      lateUpgradeCallbackRan: false,
      makeSuccessRequest: (value) => {
        const req = { result: value };
        Object.defineProperty(req, 'onsuccess', {
          set(fn) { setTimeout(fn, 0); }
        });
        return req;
      },
      makeErrorRequest: (name, message) => {
        const req = { error: { name: name, message: message } };
        Object.defineProperty(req, 'onerror', {
          set(fn) { setTimeout(fn, 0); }
        });
        return req;
      },
      makeAbortTransaction: (name, message) => {
        const tx = { error: { name: name, message: message } };
        Object.defineProperty(tx, 'onabort', {
          set(fn) { setTimeout(fn, 0); }
        });
        return tx;
      },
      install: (mode, name, message, lateDelay) => {
        globalThis.__fakeIdb.lateClosed = false;
        globalThis.__fakeIdb.lateUpgradeAborted = false;
        globalThis.__fakeIdb.lateUpgradeCallbackRan = false;
        globalThis.indexedDB = {
          open: (dbname, version) => {
            if (mode === 'throw') {
              const error = new Error(message);
              error.name = name;
              throw error;
            }
            const req = {};
            if (mode === 'error') {
              req.error = { name: name, message: message };
              Object.defineProperty(req, 'onerror', {
                set(fn) { setTimeout(fn, 0); }
              });
            } else if (mode === 'success') {
              req.result = { close: () => {} };
              Object.defineProperty(req, 'onsuccess', {
                set(fn) { setTimeout(fn, 0); }
              });
            } else if (mode === 'blocked_forever') {
              Object.defineProperty(req, 'onblocked', {
                set(fn) { setTimeout(fn, 0); }
              });
            } else if (mode === 'blocked_late_success') {
              req.result = {
                close: () => { globalThis.__fakeIdb.lateClosed = true; }
              };
              Object.defineProperty(req, 'onsuccess', {
                set(fn) { setTimeout(fn, lateDelay); }
              });
              Object.defineProperty(req, 'onblocked', {
                set(fn) { setTimeout(fn, 0); }
              });
            } else if (mode === 'blocked_late_upgrade') {
              req.transaction = {
                abort: () => {
                  globalThis.__fakeIdb.lateUpgradeAborted = true;
                }
              };
              Object.defineProperty(req, 'onupgradeneeded', {
                set(fn) {
                  setTimeout(() => {
                    fn({ oldVersion: 1, newVersion: 2 });
                  }, lateDelay);
                }
              });
              Object.defineProperty(req, 'onblocked', {
                set(fn) { setTimeout(fn, 0); }
              });
            }
            return req;
          }
        };
      },
      uninstall: () => { delete globalThis.indexedDB; },
      waitTimer: (ms) => new Promise((resolve) => {
        setTimeout(() => { resolve({ ok: true, value: true }); }, ms);
      })
    };
  JS

  def setup
    skip "wasm only" unless wasm?
    JS.global.eval(FAKE_JS)
  end

  def fakes
    JS.global[:__fakeIdb]
  end

  def with_fake_idb(mode, name = "", message = "", late_delay = 0)
    fakes.install(mode, name, message, late_delay)
    begin
      yield
    ensure
      fakes.uninstall
    end
  end

  # ---- request path ----

  def test_tagged_request_success
    req = fakes.makeSuccessRequest(42)
    promise = IndexedDB::Helper.request_to_promise(req)
    assert_equal(42, IndexedDB.__unwrap(promise.await).to_i)
  end

  def test_tagged_request_error_is_typed
    req = fakes.makeErrorRequest("QuotaExceededError", "quota is full")
    promise = IndexedDB::Helper.request_to_promise(req)
    caught = nil
    begin
      IndexedDB.__unwrap(promise.await)
    rescue IndexedDB::QuotaExceededError => e
      caught = e
    end
    assert_not_nil(caught)
    assert_equal("QuotaExceededError", caught.name)
    assert_true(caught.message.include?("quota is full"))
  end

  def test_tagged_request_unknown_name_keeps_name
    req = fakes.makeErrorRequest("SomethingWeirdError", "who knows")
    promise = IndexedDB::Helper.request_to_promise(req)
    caught = nil
    begin
      IndexedDB.__unwrap(promise.await)
    rescue IndexedDB::RequestError => e
      caught = e
    end
    assert_not_nil(caught)
    assert_equal(IndexedDB::RequestError, caught.class)
    assert_equal("SomethingWeirdError", caught.name)
  end

  # ---- transaction path ----

  def test_tagged_transaction_abort_is_typed
    tx = fakes.makeAbortTransaction("AbortError", "rolled back")
    promise = IndexedDB::Helper.transaction_to_promise(tx)
    caught = nil
    begin
      IndexedDB.__unwrap(promise.await)
    rescue IndexedDB::AbortError => e
      caught = e
    end
    assert_not_nil(caught)
    assert_equal("AbortError", caught.name)
  end

  # ---- open path: availability-only fallback ----

  def test_open_security_error_falls_back_to_in_memory
    with_fake_idb("error", "SecurityError", "denied") do
      db = IndexedDB.open("bridge_sec_fb")
      assert_true(db.is_a?(IndexedDB::InMemoryDatabase))
    end
  end

  def test_open_security_error_raises_without_fallback
    with_fake_idb("error", "SecurityError", "denied") do
      assert_raise(IndexedDB::SecurityError) do
        IndexedDB.open("bridge_sec_raise", fallback: false)
      end
    end
  end

  def test_open_synchronous_security_error_falls_back
    with_fake_idb("throw", "SecurityError", "opaque origin") do
      db = IndexedDB.open("bridge_sync_sec_fb")
      assert_true(db.is_a?(IndexedDB::InMemoryDatabase))
    end
  end

  def test_open_synchronous_security_error_is_typed_without_fallback
    with_fake_idb("throw", "SecurityError", "opaque origin") do
      caught = nil
      begin
        IndexedDB.open("bridge_sync_sec_raise", fallback: false)
      rescue IndexedDB::SecurityError => e
        caught = e
      end
      assert_not_nil(caught)
      assert_equal("SecurityError", caught.name)
      assert_true(caught.message.include?("opaque origin"))
    end
  end

  def test_open_invalid_state_error_falls_back
    with_fake_idb("error", "InvalidStateError", "no storage here") do
      db = IndexedDB.open("bridge_ise_fb")
      assert_true(db.is_a?(IndexedDB::InMemoryDatabase))
    end
  end

  def test_open_quota_error_never_falls_back
    with_fake_idb("error", "QuotaExceededError", "disk full") do
      assert_raise(IndexedDB::QuotaExceededError) do
        IndexedDB.open("bridge_quota", fallback: true)
      end
    end
  end

  def test_open_success_returns_real_database
    with_fake_idb("success") do
      db = IndexedDB.open("bridge_ok")
      assert_true(db.is_a?(IndexedDB::Database))
    end
  end

  # ---- open path: bounded onblocked wait ----

  def test_open_blocked_times_out_as_blocked_error
    with_fake_idb("blocked_forever") do
      assert_raise(IndexedDB::BlockedError) do
        IndexedDB.open("bridge_blocked", blocked_timeout_ms: 50)
      end
    end
  end

  def test_open_blocked_late_success_closes_connection
    with_fake_idb("blocked_late_success", "", "", 150) do
      assert_raise(IndexedDB::BlockedError) do
        IndexedDB.open("bridge_blocked_late", blocked_timeout_ms: 50)
      end
      # The open succeeds ~100ms after the timeout already settled the
      # promise; the bridge must close that orphan connection immediately.
      # Awaiting short JS timers (not Ruby sleep) keeps the JS event loop
      # turning so the pending late-success setTimeout can fire; one long
      # await would trip the test runner's idle-exit bound instead.
      i = 0
      while i < 30
        break if fakes[:lateClosed].to_s == "true"
        fakes.waitTimer(10).await
        i += 1
      end
      assert_equal("true", fakes[:lateClosed].to_s)
    end
  end

  def test_open_blocked_late_upgrade_is_aborted
    with_fake_idb("blocked_late_upgrade", "", "", 150) do
      assert_raise(IndexedDB::BlockedError) do
        IndexedDB.open("bridge_blocked_late_upgrade", version: 2,
                       blocked_timeout_ms: 50) do |_db, _old_v, _new_v|
          fakes[:lateUpgradeCallbackRan] = true
        end
      end
      i = 0
      while i < 30
        break if fakes[:lateUpgradeAborted].to_s == "true"
        fakes.waitTimer(10).await
        i += 1
      end
      assert_equal("true", fakes[:lateUpgradeAborted].to_s)
      assert_equal("false", fakes[:lateUpgradeCallbackRan].to_s)
    end
  end

  # ---- open path: callback registry cleanup ----

  def test_open_failure_cleans_callback_registry
    with_fake_idb("error", "SecurityError", "denied") do
      size_before = JS::Object::CALLBACKS.size
      assert_raise(IndexedDB::SecurityError) do
        IndexedDB.open("bridge_cleanup", fallback: false) do |db, old_v, new_v|
          # never runs; exists to force callback registration
        end
      end
      assert_equal(size_before, JS::Object::CALLBACKS.size)
    end
  end
end
