class SQLite3
  class Statement
    # Positional values bind in order, a Hash binds by name; an Array of
    # values counts as the values (bind_params([1, 2]) binds 1 and 2)
    def bind_params(*bind_vars)
      bind_vars = bind_vars.flatten
      index = 1
      bvi = 0
      while bvi < bind_vars.size
        var = bind_vars[bvi]
        if var.is_a?(Hash)
          keys = var.keys
          ki = 0
          while ki < keys.size
            key = keys[ki]
            bind_param(key, var[key])
            ki += 1
          end
        else
          bind_param(index, var)
          index += 1
        end
        bvi += 1
      end
      bind_vars
    end

    def execute(*bind_vars)
      reset! if active? || done?

      bind_params(*bind_vars) unless bind_vars.empty?
      @results = ResultSet.new(@connection, self)

      step if 0 == column_count

      yield @results if block_given?
      @results
    end

    def active?
      !done?
    end

    def columns
      get_metadata unless @columns
      return @columns
    end

    # Each remaining row, as an Array
    def each
      while true
        row = step
        break if row.nil? || done?
        yield row
      end
      self
    end

    # The remaining rows, as Arrays
    def to_a
      rows = [] #: Array[untyped]
      while true
        row = step
        break if row.nil? || done?
        rows << row
      end
      rows
    end

    def types
      must_be_open!
      get_metadata unless @types
      @types
    end

    def must_be_open!
      if closed?
        raise SQLite3::Exception.new("cannot use a closed statement")
      end
    end

    def get_metadata
      @columns = Array.new(column_count)
      @types = Array.new(column_count)
      column = 0
      while column < column_count
        @columns[column] = column_name(column)
        @types[column] = column_decltype(column)
        column += 1
      end
    end
  end
end

