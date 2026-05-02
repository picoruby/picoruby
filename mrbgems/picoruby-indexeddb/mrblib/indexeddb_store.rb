module IndexedDB
  # Wraps an IDBObjectStore. Two modes:
  #
  #   * upgrading: true  - constructed during the upgrade callback with an
  #     explicit IDBObjectStore from the versionchange transaction. Schema
  #     mutations (create_index) are valid. Await is forbidden.
  #
  #   * upgrading: false - normal runtime mode. Each operation opens a fresh
  #     one-shot transaction, awaits a single Promise, and returns. Safe by
  #     construction (transaction auto-commits after one await).
  class Store
    attr_reader :name

    def initialize(database, name, js_store: nil, upgrading: false)
      @database = database
      @name = name
      @js_store = js_store
      @upgrading = upgrading
    end

    def _js_store
      @js_store
    end

    # ---- One-shot autocommit operations ------------------------------------

    # Retrieve a value by primary key. Returns nil if not found.
    def get(key)
      _with_readonly_store do |store|
        req = store.get(_to_js_key(key))
        _await_request(req)
      end
    end

    # Store a value. If key_path was set on the store, the key is extracted
    # from the value. Otherwise, pass `key:` explicitly.
    #
    #   store.put({ id: 1, name: 'Alice' })         # key_path: 'id'
    #   store.put('some value', key: 'mykey')       # out-of-line key
    def put(value, key: nil)
      _with_readwrite_store do |store|
        js_value = JS::Bridge.to_js(value)
        req = if key.nil?
                store.put(js_value)
              else
                store.put(js_value, _to_js_key(key))
              end
        _await_request(req)
      end
    end

    # Delete by primary key.
    def delete(key)
      _with_readwrite_store do |store|
        req = store.delete(_to_js_key(key))
        _await_request(req)
      end
      nil
    end

    # Count records. Optionally pass a key or KeyRange.
    def count(key_or_range = nil)
      _with_readonly_store do |store|
        req = if key_or_range.nil?
                store.count
              else
                store.count(_to_js_key(key_or_range))
              end
        _await_request(req).to_i
      end
    end

    # Remove all records from the store.
    def clear
      _with_readwrite_store do |store|
        req = store.clear
        _await_request(req)
      end
      nil
    end

    # Return all primary keys as an Array.
    def keys(range = nil)
      _with_readonly_store do |store|
        req = if range.nil?
                store.getAllKeys
              else
                store.getAllKeys(_to_js_key(range))
              end
        js_arr = _await_request(req)
        _js_array_to_ruby(js_arr)
      end
    end

    # Return all values as an Array.
    def to_a(range = nil)
      _with_readonly_store do |store|
        req = if range.nil?
                store.getAll
              else
                store.getAll(_to_js_key(range))
              end
        js_arr = _await_request(req)
        _js_array_to_ruby(js_arr)
      end
    end

    # Iterate over [key, value] pairs.
    # Note: Due to IndexedDB transaction semantics (transactions auto-commit
    # when the JS event loop becomes idle, and await yields to the event loop),
    # cursor-based iteration is not possible. This method uses getAll/getAllKeys
    # to fetch all data in one transaction, then iterates in Ruby.
    def each(range = nil, direction: :next)
      return to_enum(:each, range, direction: direction) unless block_given?

      all_keys = keys(range)
      all_values = to_a(range)

      # Build pairs manually (Array#zip not available in PicoRuby)
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

    # ---- Index access ------------------------------------------------------

    # Returns an Index object for the named secondary index.
    def index(index_name)
      Index.new(self, index_name.to_s)
    end

    # ---- Schema mutations (upgrade only) -----------------------------------

    # Create a secondary index. Valid only inside the upgrade block.
    def create_index(index_name, key_path, unique: false, multi_entry: false)
      _ensure_upgrading!('create_index')
      opts = JS.global.create_object
      opts[:unique] = unique
      opts[:multiEntry] = multi_entry
      @js_store.createIndex(index_name.to_s, key_path.to_s, opts) # steep:ignore
      nil
    end

    # Delete a secondary index. Valid only inside the upgrade block.
    def delete_index(index_name)
      _ensure_upgrading!('delete_index')
      @js_store.deleteIndex(index_name.to_s) # steep:ignore
      nil
    end

    # List index names on this store.
    def index_names
      js_store = @js_store || _get_readonly_store
      list = js_store[:indexNames]
      n = list[:length].to_i
      result = [] #: Array[String]
      i = 0
      while i < n
        result << list.item(i).to_s
        i += 1
      end
      result
    end

    # ---- Internal helpers --------------------------------------------------

    def _with_readonly_store
      _ensure_not_upgrading!
      tx = @database._js_db.transaction(@name, 'readonly')
      store = tx.objectStore(@name)
      yield store
    end

    def _with_readwrite_store
      _ensure_not_upgrading!
      tx = @database._js_db.transaction(@name, 'readwrite')
      store = tx.objectStore(@name)
      yield store
    end

    def _get_readonly_store
      tx = @database._js_db.transaction(@name, 'readonly')
      tx.objectStore(@name)
    end

    def _await_request(req)
      promise = Helper._request_to_promise(req)
      promise.await
    end

    def _to_js_key(key)
      # KeyRange objects are already JS::Object; pass through
      return key if key.is_a?(JS::Object)
      JS::Bridge.to_js(key)
    end

    def _js_to_ruby(js_val)
      return nil if js_val.nil?
      # Primitives auto-convert; objects need JSON round-trip for deep conversion
      if js_val.is_a?(JS::Object)
        json_str = JS.global[:JSON].stringify(js_val).to_s # steep:ignore
        JSON.parse(json_str)
      else
        js_val
      end
    end

    def _js_array_to_ruby(js_arr)
      if js_arr.nil?
        return [] #: Array[untyped]
      end
      n = js_arr[:length].to_i
      result = [] #: Array[untyped]
      i = 0
      while i < n
        result << _js_to_ruby(js_arr[i])
        i += 1
      end
      result
    end

    def _ensure_upgrading!(op)
      return if @upgrading
      raise SchemaError, "#{op} can only be called inside the upgrade block"
    end

    def _ensure_not_upgrading!
      return unless @upgrading
      raise SchemaError, "Cannot perform data operations during upgrade; use one-shot operations after open returns"
    end
  end
end
