module IndexedDB
  # Single-store key/value facade over a Database. Useful when the application
  # only needs a persistent Hash-like surface and does not care about schemas
  # or indexes.
  #
  #   kv = IndexedDB::KVS.open('user_prefs')
  #   kv['theme'] = 'dark'
  #   kv['theme']         #=> 'dark'
  #   kv.delete('theme')
  #   kv.each { |k, v| ... }
  #
  # Every operation autocommits via the underlying Database/InMemoryDatabase
  # one-shot transaction semantics.
  class KVS
    DEFAULT_STORE = 'kv'.freeze

    # Open (or create) a KVS-backed database. If the store does not exist yet,
    # it is created at version 1. Pass `store:` to use a non-default store
    # name (useful when sharing one Database between several KVS instances).
    def self.open(name, store: DEFAULT_STORE, fallback: true)
      store_name = store.to_s
      db = IndexedDB.open(name, version: 1, fallback: fallback) do |d, _old_v, _new_v|
        d.create_store(store_name) unless d.has_store?(store_name)
      end
      new(db, store_name)
    end

    attr_reader :name

    def initialize(database, store_name)
      @database = database
      @name = store_name
    end

    def [](key)
      store.get(key.to_s)
    end

    def []=(key, value)
      store.put(value, key: key.to_s)
      value
    end

    def delete(key)
      store.delete(key.to_s)
      nil
    end

    def has_key?(key)
      !store.get(key.to_s).nil?
    end

    def size
      store.count
    end

    alias length size

    def empty?
      size == 0
    end

    def clear
      store.clear
      nil
    end

    def keys
      store.keys.map { |k| k.to_s }
    end

    def values
      store.to_a
    end

    def each(&block)
      return to_enum(:each) unless block

      store.each do |pair|
        block.call(pair)
      end
    end

    def to_h
      result = {} #: Hash[String, untyped]
      store.each do |pair|
        key = pair[0]
        value = pair[1]
        result[key.to_s] = value
      end
      result
    end

    def close
      @database.close
    end

    def database
      @database
    end

    private

    def store
      s = @database.store(@name)
      raise Error, "store '#{@name}' missing from KVS database" unless s
      s
    end
  end
end
