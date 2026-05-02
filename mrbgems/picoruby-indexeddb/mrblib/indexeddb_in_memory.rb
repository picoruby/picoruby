module IndexedDB
  # Hash-backed fallback when IndexedDB is unavailable.
  # Provides the same API surface as Database for transparent fallback.
  class InMemoryDatabase
    @stores = {} #: Hash[[String, String], Hash[untyped, untyped]]
    @meta = {}   #: Hash[String, Hash[Symbol, untyped]]

    class << self
      attr_accessor :stores, :meta
    end

    def self.open(name, version, &block)
      @stores ||= {} #: Hash[[String, String], Hash[untyped, untyped]]
      @meta ||= {} #: Hash[String, Hash[Symbol, untyped]]

      existing_version = @meta.dig(name, :version) || 0

      db = new(name, version)

      if block && existing_version < version
        block.call(db, existing_version, version)
        db.mark_upgrade_done
      else
        db.mark_upgrade_done
      end

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
      ensure_upgrading!('create_store')
      store_name = name.to_s

      self.class.stores[[@name, store_name]] ||= {} #: Hash[untyped, untyped]

      self.class.meta[@name] ||= {} #: Hash[Symbol, untyped]
      self.class.meta[@name][:stores] ||= {} #: Hash[String, Hash[Symbol, untyped]]
      self.class.meta[@name][:stores][store_name] = {
        key_path: key_path&.to_s,
        auto_increment: auto_increment,
        next_key: 1,
        indexes: {} #: Hash[String, Hash[Symbol, untyped]]
      }

      @store_names << store_name unless @store_names.include?(store_name)

      InMemoryStore.new(self, store_name, upgrading: true)
    end

    def delete_store(name)
      ensure_upgrading!('delete_store')
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

    # In-memory batch: synchronous, no real transaction. The block runs
    # against InMemoryBatchView which mirrors Batch::StoreView semantics.
    def batch(store_names, mode: :readwrite, &block)
      raise SchemaError, "batch cannot be called inside the upgrade block" if @upgrading
      raise ArgumentError, "batch requires a block" unless block
      names = store_names.is_a?(Array) ? store_names.map { |n| n.to_s } : [store_names.to_s]
      tx = InMemoryBatch.new(self, names, mode)
      block.call(tx)
      nil
    end

    def mark_upgrade_done
      @upgrading = false
    end

    def upgrading?
      @upgrading
    end

    def store_meta(store_name)
      self.class.meta.dig(@name, :stores, store_name) || {} #: Hash[Symbol, untyped]
    end

    def store_data(store_name)
      self.class.stores[[@name, store_name]] ||= {} #: Hash[untyped, untyped]
    end

    private

    def ensure_upgrading!(op)
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
      data[key]
    end

    def put(value, key: nil)
      m = meta
      actual_key = if key.nil? && m[:key_path]
                     extract_key(value, m[:key_path])
                   elsif key.nil? && m[:auto_increment]
                     k = m[:next_key]
                     m[:next_key] = k + 1
                     k
                   else
                     key
                   end

      raise Error, "No key provided and no key_path/auto_increment" if actual_key.nil?

      stored_value = deep_copy(value)
      data[actual_key] = stored_value
      actual_key
    end

    def delete(key)
      data.delete(key)
      nil
    end

    def count(key_or_range = nil)
      if key_or_range.nil?
        data.size
      else
        data.key?(key_or_range) ? 1 : 0
      end
    end

    def clear
      data.clear
      nil
    end

    def keys(range = nil)
      if range.nil?
        data.keys
      else
        data.keys
      end
    end

    def to_a(range = nil)
      if range.nil?
        data.values
      else
        data.values
      end
    end

    def each(range = nil, direction: :next)
      return to_enum(:each, range, direction: direction) unless block_given?

      pairs = data.to_a
      pairs = pairs.reverse if direction == :prev || direction == 'prev'

      pairs.each do |key, value|
        yield [key, value]
      end
    end

    # ---- Index access (stub) -----------------------------------------------

    def index(index_name)
      InMemoryIndex.new(self, index_name.to_s)
    end

    # ---- Schema mutations (upgrade only) -----------------------------------

    def create_index(index_name, key_path, unique: false, multi_entry: false)
      ensure_upgrading!('create_index')
      m = meta
      m[:indexes] ||= {}
      m[:indexes][index_name.to_s] = {
        key_path: key_path.to_s,
        unique: unique,
        multi_entry: multi_entry
      }
      nil
    end

    def delete_index(index_name)
      ensure_upgrading!('delete_index')
      m = meta
      m[:indexes]&.delete(index_name.to_s)
      nil
    end

    def index_names
      m = meta
      (m[:indexes] || {}).keys
    end

    # ---- Cross-class internals (called by InMemoryIndex) -------------------

    def data
      @database.store_data(@name)
    end

    def meta
      @database.store_meta(@name)
    end

    private

    def extract_key(value, key_path)
      if value.is_a?(Hash)
        value[key_path] || value[key_path.to_sym]
      else
        nil
      end
    end

    def deep_copy(obj)
      case obj
      when Hash
        obj.each_with_object({} #: Hash[untyped, untyped]
        ) { |(k, v), h| h[k] = deep_copy(v) }
      when Array
        obj.map { |v| deep_copy(v) }
      else
        obj
      end
    end

    def ensure_upgrading!(op)
      return if @upgrading
      raise SchemaError, "#{op} can only be called inside the upgrade block"
    end
  end

  # In-memory batch wrapper. Mirrors Batch's API surface so the same user
  # block runs against either backend. Reads still raise AwaitInBatchError
  # so users do not write code that only works on the in-memory path.
  class InMemoryBatch
    def initialize(database, store_names, mode)
      @database = database
      @store_names = store_names
      @mode = mode.to_s
      @views = {} #: Hash[String, InMemoryBatchView]
    end

    def [](store_name)
      key = store_name.to_s
      view = @views[key]
      return view if view
      raise Error, "store '#{key}' not in this batch's scope" unless @store_names.include?(key)
      v = InMemoryBatchView.new(@database, key)
      @views[key] = v
      v
    end
  end

  class InMemoryBatchView
    attr_reader :name

    def initialize(database, name)
      @database = database
      @name = name
      @store = InMemoryStore.new(database, name, upgrading: false)
    end

    def put(value, key: nil)
      @store.put(value, key: key)
      nil
    end

    def add(value, key: nil)
      @store.put(value, key: key)
      nil
    end

    def delete(key)
      @store.delete(key)
      nil
    end

    def clear
      @store.clear
      nil
    end

    def get(_key)
      raise AwaitInBatchError, "get cannot be called inside batch"
    end

    def count(_key_or_range = nil)
      raise AwaitInBatchError, "count cannot be called inside batch"
    end

    def keys(_range = nil)
      raise AwaitInBatchError, "keys cannot be called inside batch"
    end

    def to_a(_range = nil)
      raise AwaitInBatchError, "to_a cannot be called inside batch"
    end

    def each(*_args)
      raise AwaitInBatchError, "each cannot be called inside batch"
    end

    def index(_name)
      raise AwaitInBatchError, "index cannot be called inside batch"
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
      m = @store.meta
      index_meta = m[:indexes]&.dig(@name)
      return nil unless index_meta

      key_path = index_meta[:key_path]
      @store.data.each_value do |value|
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
