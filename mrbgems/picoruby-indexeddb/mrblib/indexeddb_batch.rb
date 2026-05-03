module IndexedDB
  # Atomic multi-store transaction. Created by Database#batch.
  #
  # The block enqueues write requests synchronously without awaiting; on block
  # exit, a single Promise tied to the transaction's oncomplete event is awaited
  # (committing the whole group or rolling back on error).
  #
  # Inside the block, await-bearing operations (get / count / keys / to_a / each
  # / index) are forbidden because each await yields to the JS event loop, which
  # auto-commits the transaction and corrupts state. They raise AwaitInBatchError.
  class Batch
    def initialize(database, store_names, mode)
      @database = database
      js_db = database.js_db
      mode_str = mode.to_s

      tx = if store_names.length == 1
             js_db.transaction(store_names[0], mode_str)
           else
             names_arr = JS.global.create_array
             store_names.each { |n| names_arr.push(n) }
             js_db.transaction(names_arr, mode_str)
           end

      @tx = tx
      @views = {} #: Hash[String, StoreView]
    end

    # Returns a StoreView for the named store. Cached per-batch.
    def [](store_name)
      key = store_name.to_s
      view = @views[key]
      return view if view
      js_store = @tx.objectStore(key)
      v = StoreView.new(js_store, key)
      @views[key] = v
      v
    end

    def await_complete
      promise = Helper.transaction_to_promise(@tx)
      promise.await
      nil
    end

    # Restricted Store-like interface usable inside a batch block. Only
    # operations that do NOT need to await an IDBRequest are exposed. Read
    # methods raise AwaitInBatchError so users get a clear failure mode.
    class StoreView
      attr_reader :name

      def initialize(js_store, name)
        @js_store = js_store
        @name = name
      end

      def put(value, key: nil)
        js_value = JS::Bridge.to_js(value)
        if key.nil?
          @js_store.put(js_value)
        else
          @js_store.put(js_value, JS::Bridge.to_js(key))
        end
        nil
      end

      def add(value, key: nil)
        js_value = JS::Bridge.to_js(value)
        if key.nil?
          @js_store.add(js_value)
        else
          @js_store.add(js_value, JS::Bridge.to_js(key))
        end
        nil
      end

      def delete(key)
        @js_store.delete(JS::Bridge.to_js(key))
        nil
      end

      def clear
        @js_store.clear
        nil
      end

      def get(_key)
        raise AwaitInBatchError, "get cannot be called inside batch; perform reads outside the batch block"
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
  end
end
