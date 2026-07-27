begin
  require 'js'
rescue LoadError
  # Not available outside the wasm build; the in-memory fallback still works.
end

module IndexedDB
  # How long an onblocked open may wait before it fails with BlockedError.
  BLOCKED_TIMEOUT_MS = 5000

  # True when the browser exposes globalThis.indexedDB.
  def self.available?
    !JS.global[:indexedDB].nil?
  end

  # Unwrap a tagged bridge result ({ ok: true, value: } | { ok: false,
  # name:, message: }) into the value or a typed exception. The EM_JS
  # helpers always RESOLVE tags -- a rejection would reach Ruby as a bare
  # message string with the error class lost.
  def self.__unwrap(tagged)
    err_name = tagged[:name]
    return tagged[:value] if err_name.nil?
    name = err_name.to_s
    message = tagged[:message].to_s
    if name == "PicoRubyBlockedTimeout"
      raise BlockedError.new(message)
    end
    raise error_for(name, message)
  end

  # Open (and upgrade) a database.
  #
  #   IndexedDB.open('mydb', version: 2) do |db, old_v, new_v|
  #     db.create_store('users', key_path: 'id') if old_v < 1
  #     db.create_store('cache')                  if old_v < 2
  #   end
  #
  # The block runs synchronously inside onupgradeneeded if (and only if)
  # a schema upgrade is triggered. Schema mutations are valid only there.
  # `await` MUST NOT be used inside the upgrade block.
  #
  # When IndexedDB is unusable in this environment, the fallback is an
  # in-memory implementation with the same API surface (no persistence).
  # `fallback: true` applies ONLY to availability: a missing
  # globalThis.indexedDB, or an open failing with SecurityError /
  # InvalidStateError (private modes). Quota, version, and data errors
  # always raise typed -- silently substituting an empty in-memory store
  # would masquerade as losing previously persisted data.
  def self.open(name, version: 1, fallback: true, blocked_timeout_ms: BLOCKED_TIMEOUT_MS, &block)
    if available?
      begin
        open_real(name, version, blocked_timeout_ms, &block)
      rescue SecurityError, InvalidStateError => e
        raise e unless fallback
        open_fallback(name, version, &block)
      end
    elsif fallback
      open_fallback(name, version, &block)
    else
      raise NotSupportedError, "IndexedDB is not available in this environment"
    end
  end

  class << self
    private

    def open_real(name, version, blocked_timeout_ms, &block)
      callback_id = 0
      if block
        user_block = block
        # @type var user_block: ^(IndexedDB::db_like, Integer, Integer) -> void
        proc_obj = ->(db_ref, old_v, new_v) do
          upgrade_db = Database.new(db_ref, name: name, upgrading: true)
          user_block.call(upgrade_db, old_v, new_v)
          upgrade_db.mark_upgrade_done
          nil
        end
        callback_id = proc_obj.object_id
        JS::Object::CALLBACKS[callback_id] = proc_obj
      end

      raw_db = nil
      begin
        promise = Helper.open_with_upgrade(name.to_s, version.to_i, callback_id,
                                           blocked_timeout_ms.to_i)
        raw_db = IndexedDB.__unwrap(promise.await)
      ensure
        # Without the ensure, a raise out of await/unwrap would leak the
        # registration forever.
        JS::Object::CALLBACKS.delete(callback_id) if callback_id != 0
      end
      Database.new(raw_db, name: name, upgrading: false)
    end

    def open_fallback(name, version, &block)
      InMemoryDatabase.open(name, version, &block)
    end
  end
end
