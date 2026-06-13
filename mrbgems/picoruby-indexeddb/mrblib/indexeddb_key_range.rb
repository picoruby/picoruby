module IndexedDB
  # Thin wrapper over IDBKeyRange for range queries.
  # Returns JS::Object references that can be passed directly to
  # Store#each, Store#keys, Store#to_a, Index methods, etc.
  module KeyRange
    # Match exactly one key.
    def self.only(value)
      idb_key_range = JS.global[:IDBKeyRange]
      idb_key_range.only(JS::Bridge.to_js(value)) # steep:ignore
    end

    # All keys >= lower (or > lower if exclude: true).
    def self.lower_bound(lower, exclude: false)
      idb_key_range = JS.global[:IDBKeyRange]
      idb_key_range.lowerBound(JS::Bridge.to_js(lower), exclude) # steep:ignore
    end

    # All keys <= upper (or < upper if exclude: true).
    def self.upper_bound(upper, exclude: false)
      idb_key_range = JS.global[:IDBKeyRange]
      idb_key_range.upperBound(JS::Bridge.to_js(upper), exclude) # steep:ignore
    end

    # All keys between lower and upper.
    #   exclude_lower: true  => lower < key
    #   exclude_upper: true  => key < upper
    def self.bound(lower, upper, exclude_lower: false, exclude_upper: false)
      idb_key_range = JS.global[:IDBKeyRange]
      idb_key_range.bound( # steep:ignore
        JS::Bridge.to_js(lower),
        JS::Bridge.to_js(upper),
        exclude_lower,
        exclude_upper
      )
    end
  end
end
