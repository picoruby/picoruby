module IndexedDB
  # Wraps an IDBIndex for secondary index access.
  # Provides the same read operations as Store but queries via the index.
  class Index
    attr_reader :name

    def initialize(store, name)
      @store = store
      @name = name
    end

    # Retrieve the first record matching the given index key.
    def get(key)
      @store.with_readonly_store do |js_store|
        idx = js_store.index(@name)
        req = idx.get(@store.to_js_key(key))
        @store.js_to_ruby(@store.await_request(req))
      end
    end

    # Retrieve all records matching the given index key (or range).
    def get_all(key_or_range = nil, count: nil)
      @store.with_readonly_store do |js_store|
        idx = js_store.index(@name)
        req = if key_or_range.nil?
                count.nil? ? idx.getAll : idx.getAll(nil, count) # steep:ignore
              else
                js_key = @store.to_js_key(key_or_range)
                count.nil? ? idx.getAll(js_key) : idx.getAll(js_key, count)
              end
        js_arr = @store.await_request(req)
        @store.js_array_to_ruby(js_arr)
      end
    end

    # Count records matching the key or range.
    def count(key_or_range = nil)
      @store.with_readonly_store do |js_store|
        idx = js_store.index(@name)
        req = if key_or_range.nil?
                idx.count
              else
                idx.count(@store.to_js_key(key_or_range))
              end
        @store.await_request(req).to_i
      end
    end

    # Return all values via the index order.
    def to_a(range = nil)
      get_all(range)
    end

    # Iterate over [primary_key, value] pairs via the index.
    # Uses bulk fetch (getAll/getAllKeys) due to transaction limitations.
    def each(range = nil, direction: :next)
      return to_enum(:each, range, direction: direction) unless block_given?

      all_keys = keys(range)
      all_values = to_a(range)

      pairs = [] #: Array[[untyped, untyped]]
      i = 0
      while i < all_keys.length
        pairs << [all_keys[i], all_values[i]]
        i += 1
      end

      pairs = pairs.reverse if direction == :prev || direction == 'prev'

      pairs.each do |pair|
        yield pair
      end
    end

    # Return all primary keys in index order.
    def keys(range = nil)
      @store.with_readonly_store do |js_store|
        idx = js_store.index(@name)
        req = if range.nil?
                idx.getAllKeys
              else
                idx.getAllKeys(@store.to_js_key(range))
              end
        js_arr = @store.await_request(req)
        @store.js_array_to_ruby(js_arr)
      end
    end
  end
end
