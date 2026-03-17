class SQLite3
  class Statement
    def bind_params(*bind_vars)
      index = 1
      bvi = 0
      while bvi < bind_vars.size
        var = bind_vars[bvi]
        if var.is_a?(Hash)
          var.each { |key, val| bind_param(key, val) }
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

#    def each
#      loop do
#        val = step
#        break self if done?
#        yield val
#      end
#    end

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

