class SQLite3
  class Statement
    def bind_params(*bind_vars)
      index = 1
      bind_vars.each do |var|
        if Hash === var
          var.each { |key, val| bind_param(key, val) }
        else
          bind_param(index, var)
          index += 1
        end
      end
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
#      get_metadata unless @columns
#      return @columns
    end

#    def each
#      loop do
#        val = step
#        break self if done?
#        yield val
#      end
#    end

    def types
#      must_be_open!
#      get_metadata unless @types
      @types
    end
  end
end

