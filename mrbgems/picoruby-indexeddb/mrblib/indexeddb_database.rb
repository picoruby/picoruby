module IndexedDB
  # Wraps an IDBDatabase. Two lifecycle states:
  #
  #   * upgrading: true   - constructed inside the upgrade callback. Schema
  #     mutations (create_store / delete_store / Store#create_index) are
  #     valid here and only here. Stays valid only for the duration of the
  #     onupgradeneeded handler; await is forbidden.
  #
  #   * upgrading: false  - the normal post-open state. Schema mutations
  #     raise SchemaError. Await-bearing operations on Stores are valid.
  class Database
    attr_reader :name

    def initialize(js_db, name:, upgrading:)
      @js_db = js_db
      @name = name
      @upgrading = upgrading
    end

    def closed?
      @js_db.nil?
    end

    def close
      return if closed?
      @js_db.close
      @js_db = nil
    end

    def version
      @js_db[:version].to_i
    end

    def store_names
      list = @js_db[:objectStoreNames]
      n = list[:length].to_i
      result = [] #: Array[String]
      i = 0
      while i < n
        result << list.item(i).to_s
        i += 1
      end
      result
    end

    def has_store?(name)
      store_names.include?(name.to_s)
    end

    # ---- Schema mutations (upgrade-only) -----------------------------------

    def create_store(name, key_path: nil, auto_increment: false)
      _ensure_upgrading!('create_store')
      opts = JS.global.create_object
      opts[:keyPath] = key_path.to_s if key_path
      opts[:autoIncrement] = true if auto_increment
      js_store = @js_db.createObjectStore(name.to_s, opts)
      _store_for(js_store, name.to_s)
    end

    def delete_store(name)
      _ensure_upgrading!('delete_store')
      @js_db.deleteObjectStore(name.to_s)
      nil
    end

    # ---- Accessors ---------------------------------------------------------

    # Inside upgrade context, returns a Store wrapping the upgrade-mode
    # IDBObjectStore (so create_index can be called). Outside, returns a
    # Store backed by a fresh one-shot transaction per operation.
    def store(name)
      if @upgrading
        # During upgradeneeded the implicit versionchange transaction is
        # available on the request, but the IDBObjectStore must be obtained
        # from the version-change transaction. We expose it via the JS
        # Database#transaction property which IDB sets on the open request.
        tx = @js_db[:transaction]
        if tx.nil?
          raise SchemaError, "no active versionchange transaction"
        end
        _store_for(tx.objectStore(name.to_s), name.to_s)
      else
        # Phase B: returns a Store that opens a one-shot transaction per op.
        Store.new(self, name.to_s)
      end
    end

    # Internal: build a Store wrapping a specific IDBObjectStore (used
    # during upgrade so create_store / store(name) return upgrade-mode
    # stores backed by the versionchange transaction).
    def _store_for(js_store, name)
      Store.new(self, name, js_store: js_store, upgrading: @upgrading)
    end

    # Internal: marks the upgrade phase complete. After this, the Database
    # rejects schema mutations.
    def _mark_upgrade_done
      @upgrading = false
    end

    def _js_db
      @js_db
    end

    def _upgrading?
      @upgrading
    end

    private

    def _ensure_upgrading!(op)
      return if @upgrading
      raise SchemaError, "#{op} can only be called inside the upgrade block"
    end
  end
end
