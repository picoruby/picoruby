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

    # ---- One-shot autocommit operations ------------------------------------

    # Retrieve a value by primary key. Returns nil if not found.
    def get(key)
      with_readonly_store do |store|
        req = store.get(to_js_key(key))
        await_request(req)
      end
    end

    # Store a value. If key_path was set on the store, the key is extracted
    # from the value. Otherwise, pass `key:` explicitly.
    #
    #   store.put({ id: 1, name: 'Alice' })         # key_path: 'id'
    #   store.put('some value', key: 'mykey')       # out-of-line key
    def put(value, key: nil)
      with_readwrite_store do |store|
        js_value = JS::Bridge.to_js(value)
        req = if key.nil?
                store.put(js_value)
              else
                store.put(js_value, to_js_key(key))
              end
        await_request(req)
      end
    end

    # Delete by primary key.
    def delete(key)
      with_readwrite_store do |store|
        req = store.delete(to_js_key(key))
        await_request(req)
      end
      nil
    end

    # Count records. Optionally pass a key or KeyRange.
    def count(key_or_range = nil)
      with_readonly_store do |store|
        req = if key_or_range.nil?
                store.count
              else
                store.count(to_js_key(key_or_range))
              end
        await_request(req).to_i
      end
    end

    # Remove all records from the store.
    def clear
      with_readwrite_store do |store|
        req = store.clear
        await_request(req)
      end
      nil
    end

    # Return all primary keys as an Array.
    def keys(range = nil)
      with_readonly_store do |store|
        req = if range.nil?
                store.getAllKeys
              else
                store.getAllKeys(to_js_key(range))
              end
        js_arr = await_request(req)
        js_array_to_ruby(js_arr)
      end
    end

    # Return all values as an Array.
    def to_a(range = nil)
      with_readonly_store do |store|
        req = if range.nil?
                store.getAll
              else
                store.getAll(to_js_key(range))
              end
        js_arr = await_request(req)
        js_array_to_ruby(js_arr)
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
      ensure_upgrading!('create_index')
      opts = JS.global.create_object
      opts[:unique] = unique
      opts[:multiEntry] = multi_entry
      @js_store.createIndex(index_name.to_s, key_path.to_s, opts) # steep:ignore
      nil
    end

    # Delete a secondary index. Valid only inside the upgrade block.
    def delete_index(index_name)
      ensure_upgrading!('delete_index')
      @js_store.deleteIndex(index_name.to_s) # steep:ignore
      nil
    end

    # List index names on this store.
    def index_names
      js_store = @js_store || get_readonly_store
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

    # ---- Cross-class internals (called by Index) ---------------------------

    def with_readonly_store
      ensure_not_upgrading!
      tx = @database.js_db.transaction(@name, 'readonly')
      store = tx.objectStore(@name)
      yield store
    end

    def await_request(req)
      promise = Helper.request_to_promise(req)
      promise.await
    end

    def to_js_key(key)
      return key if key.is_a?(JS::Object)
      JS::Bridge.to_js(key)
    end

    def js_array_to_ruby(js_arr)
      if js_arr.nil?
        return [] #: Array[untyped]
      end
      n = js_arr[:length].to_i
      result = [] #: Array[untyped]
      i = 0
      while i < n
        result << js_to_ruby(js_arr[i])
        i += 1
      end
      result
    end

    private

    def with_readwrite_store
      ensure_not_upgrading!
      tx = @database.js_db.transaction(@name, 'readwrite')
      store = tx.objectStore(@name)
      yield store
    end

    def get_readonly_store
      tx = @database.js_db.transaction(@name, 'readonly')
      tx.objectStore(@name)
    end

    def js_to_ruby(js_val)
      return nil if js_val.nil?
      if js_val.is_a?(JS::Object)
        json_str = JS.global[:JSON].stringify(js_val).to_s # steep:ignore
        JSON.parse(json_str)
      else
        js_val
      end
    end

    def ensure_upgrading!(op)
      return if @upgrading
      raise SchemaError, "#{op} can only be called inside the upgrade block"
    end

    def ensure_not_upgrading!
      return unless @upgrading
      raise SchemaError, "Cannot perform data operations during upgrade; use one-shot operations after open returns"
    end
  end
end
