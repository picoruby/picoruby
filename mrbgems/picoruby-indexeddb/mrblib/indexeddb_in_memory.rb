module IndexedDB
  # Hash-backed fallback when IndexedDB is unavailable.
  # Provides the same API surface as Database for transparent fallback.
  class InMemoryDatabase
    # Class instance variables for singleton storage
    @stores = {} #: Hash[[String, String], Hash[untyped, untyped]]
    @meta = {}   #: Hash[String, Hash[Symbol, untyped]]

    class << self
      attr_accessor :stores, :meta
    end

    def self.open(name, version, &block)
      @stores ||= {} #: Hash[[String, String], Hash[untyped, untyped]]
      @meta ||= {} #: Hash[String, Hash[Symbol, untyped]]

      # Check existing version
      existing_version = @meta.dig(name, :version) || 0

      db = new(name, version)

      if block && existing_version < version
        block.call(db, existing_version, version)
        db._mark_upgrade_done
      else
        db._mark_upgrade_done
      end

      # Record the version
      @meta[name] ||= {} #: Hash[Symbol, untyped]
      @meta[name][:version] = version

      db
    end

    def initialize(name, version)
      @name = name
      @version = version
      @upgrading = true
      @closed = false
      @store_names = self.class.meta.dig(name, :stores)&.keys || []
    end

    def name; @name; end
    def version; @version; end
    def closed?; @closed; end

    def close
      @closed = true
    end

    def store_names
      @store_names.dup
    end

    def has_store?(name)
      @store_names.include?(name.to_s)
    end

    # ---- Schema mutations (upgrade only) -----------------------------------

    def create_store(name, key_path: nil, auto_increment: false)
      _ensure_upgrading!('create_store')
      store_name = name.to_s

      # Initialize storage
      self.class.stores[[@name, store_name]] ||= {} #: Hash[untyped, untyped]

      # Record metadata
      self.class.meta[@name] ||= {} #: Hash[Symbol, untyped]
      self.class.meta[@name][:stores] ||= {} #: Hash[String, Hash[Symbol, untyped]]
      self.class.meta[@name][:stores][store_name] = {
        key_path: key_path&.to_s,
        auto_increment: auto_increment,
        next_key: 1,
        indexes: {} #: Hash[String, Hash[Symbol, untyped]]
      }

      @store_names << store_name unless @store_names.include?(store_name)

      # Return a store object for chaining (e.g., create_index during upgrade)
      InMemoryStore.new(self, store_name, upgrading: true)
    end

    def delete_store(name)
      _ensure_upgrading!('delete_store')
      store_name = name.to_s

      self.class.stores.delete([@name, store_name])
      self.class.meta[@name][:stores]&.delete(store_name)
      @store_names.delete(store_name)

      nil
    end

    # ---- Accessors ---------------------------------------------------------

    def store(name)
      store_name = name.to_s
      return nil unless has_store?(store_name)
      InMemoryStore.new(self, store_name, upgrading: @upgrading)
    end

    def _mark_upgrade_done
      @upgrading = false
    end

    def _upgrading?
      @upgrading
    end

    def _db_name
      @name
    end

    def _store_meta(store_name)
      self.class.meta.dig(@name, :stores, store_name) || {} #: Hash[Symbol, untyped]
    end

    def _store_data(store_name)
      self.class.stores[[@name, store_name]] ||= {} #: Hash[untyped, untyped]
    end

    private

    def _ensure_upgrading!(op)
      return if @upgrading
      raise SchemaError, "#{op} can only be called inside the upgrade block"
    end
  end

  # In-memory Store implementation with the same API as Store.
  class InMemoryStore
    attr_reader :name

    def initialize(database, name, upgrading: false)
      @database = database
      @name = name
      @upgrading = upgrading
    end

    # ---- One-shot operations -----------------------------------------------

    def get(key)
      _data[key]
    end

    def put(value, key: nil)
      meta = _meta
      actual_key = if key.nil? && meta[:key_path]
                     # Extract key from value using key_path
                     _extract_key(value, meta[:key_path])
                   elsif key.nil? && meta[:auto_increment]
                     k = meta[:next_key]
                     meta[:next_key] = k + 1
                     k
                   else
                     key
                   end

      raise Error, "No key provided and no key_path/auto_increment" if actual_key.nil?

      # Deep copy to avoid mutation issues
      stored_value = _deep_copy(value)
      _data[actual_key] = stored_value
      actual_key
    end

    def delete(key)
      _data.delete(key)
      nil
    end

    def count(key_or_range = nil)
      if key_or_range.nil?
        _data.size
      else
        # Simple key lookup
        _data.key?(key_or_range) ? 1 : 0
      end
    end

    def clear
      _data.clear
      nil
    end

    def keys(range = nil)
      if range.nil?
        _data.keys
      else
        # For simplicity, return all keys (range filtering would need more work)
        _data.keys
      end
    end

    def to_a(range = nil)
      if range.nil?
        _data.values
      else
        _data.values
      end
    end

    def each(range = nil, direction: :next)
      return to_enum(:each, range, direction: direction) unless block_given?

      pairs = _data.to_a
      pairs = pairs.reverse if direction == :prev || direction == 'prev'

      pairs.each do |key, value|
        yield [key, value]
      end
    end

    # ---- Index access (stub) -----------------------------------------------

    def index(index_name)
      # Return a simple stub that returns nil/empty for queries
      InMemoryIndex.new(self, index_name.to_s)
    end

    # ---- Schema mutations (upgrade only) -----------------------------------

    def create_index(index_name, key_path, unique: false, multi_entry: false)
      _ensure_upgrading!('create_index')
      meta = _meta
      meta[:indexes] ||= {}
      meta[:indexes][index_name.to_s] = {
        key_path: key_path.to_s,
        unique: unique,
        multi_entry: multi_entry
      }
      nil
    end

    def delete_index(index_name)
      _ensure_upgrading!('delete_index')
      meta = _meta
      meta[:indexes]&.delete(index_name.to_s)
      nil
    end

    def index_names
      meta = _meta
      (meta[:indexes] || {}).keys
    end

    # ---- Internal ----------------------------------------------------------

    def _data
      @database._store_data(@name)
    end

    def _meta
      @database._store_meta(@name)
    end

    def _extract_key(value, key_path)
      # Simple single-level key extraction
      if value.is_a?(Hash)
        value[key_path] || value[key_path.to_sym]
      else
        nil
      end
    end

    def _deep_copy(obj)
      case obj
      when Hash
        obj.each_with_object({} #: Hash[untyped, untyped]
        ) { |(k, v), h| h[k] = _deep_copy(v) }
      when Array
        obj.map { |v| _deep_copy(v) }
      else
        obj
      end
    end

    def _ensure_upgrading!(op)
      return if @upgrading
      raise SchemaError, "#{op} can only be called inside the upgrade block"
    end
  end

  # Minimal Index stub for in-memory fallback.
  class InMemoryIndex
    attr_reader :name

    def initialize(store, name)
      @store = store
      @name = name
    end

    def get(key)
      # Linear scan for matching value
      meta = @store._meta
      index_meta = meta[:indexes]&.dig(@name)
      return nil unless index_meta

      key_path = index_meta[:key_path]
      @store._data.each_value do |value|
        if value.is_a?(Hash)
          indexed_val = value[key_path] || value[key_path.to_sym]
          return value if indexed_val == key
        end
      end
      nil
    end

    def to_a
      @store.to_a
    end

    def each(&block)
      @store.each(&block)
    end
  end
end
