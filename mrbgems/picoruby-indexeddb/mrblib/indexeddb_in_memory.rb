module IndexedDB
  # Phase A: stub for the Hash-backed fallback. Database#open delegates here
  # when IndexedDB.available? is false. The actual KVS-style methods are
  # added in Phase B alongside the real Store.
  class InMemoryDatabase
    def self.open(name, version, &block)
      db = new(name, version)
      if block
        block.call(db, 0, version)
        db._mark_upgrade_done
      end
      db
    end

    def initialize(name, version)
      @name = name
      @version = version
      @upgrading = true
    end

    def name; @name; end
    def version; @version; end
    def closed?; false; end
    def close; end
    def store_names; []; end
    def has_store?(_); false; end

    def create_store(name, key_path: nil, auto_increment: false)
      _ensure_upgrading!('create_store')
      # Phase B will register the store in the singleton hash. For Phase A
      # we accept the call so smoke tests do not crash in fallback mode.
      nil
    end

    def delete_store(name)
      _ensure_upgrading!('delete_store')
      nil
    end

    def store(name)
      nil
    end

    def _mark_upgrade_done
      @upgrading = false
    end

    private

    def _ensure_upgrading!(op)
      return if @upgrading
      raise SchemaError, "#{op} can only be called inside the upgrade block"
    end
  end
end
