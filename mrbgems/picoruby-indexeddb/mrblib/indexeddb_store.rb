module IndexedDB
  # Phase A: stub. Only the constructor and accessors used by Database
  # during upgrade are defined here. The one-shot autocommit operations
  # (#get, #put, #delete, #count, #clear, #keys, #to_a, #each, #index,
  # #create_index) are added in Phase B.
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
  end
end
