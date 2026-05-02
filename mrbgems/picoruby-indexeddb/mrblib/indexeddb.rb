begin
  require 'js'
rescue LoadError
  # Not available outside the wasm build; the in-memory fallback still works.
end

module IndexedDB
  # True when the browser exposes globalThis.indexedDB.
  def self.available?
    !JS.global[:indexedDB].nil?
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
  # When `IndexedDB.available?` is false, the call falls back to an
  # in-memory implementation with the same API surface (no persistence).
  def self.open(name, version: 1, fallback: true, &block)
    if available?
      open_real(name, version, &block)
    elsif fallback
      open_fallback(name, version, &block)
    else
      raise NotSupportedError, "IndexedDB is not available in this environment"
    end
  end

  class << self
    private

    def open_real(name, version, &block)
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

      promise = Helper.open_with_upgrade(name.to_s, version.to_i, callback_id)
      raw_db = promise.await
      JS::Object::CALLBACKS.delete(callback_id) if callback_id != 0
      Database.new(raw_db, name: name, upgrading: false)
    end

    def open_fallback(name, version, &block)
      InMemoryDatabase.open(name, version, &block)
    end
  end
end
