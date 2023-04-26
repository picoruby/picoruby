class SQLite3
  class ResultSet

    def initialize(db, stmt)
      @db = db
      @stmt = stmt
    end

    def reset(*bind_params)
      @stmt.reset!
      @stmt.bind_params(*bind_params)
      @eof = false
    end

    def eof?
      @stmt.done?
    end

    def close
      @stmt.close
    end

    def closed?
      @stmt.closed?
    end

    def types
      @stmt.types
    end

    def columns
      @stmt.columns
    end

    def each
      while node = self.next
        yield node
      end
    end

    def next
    #  if @db.results_as_hash
    #    return next_hash
    #  end
      row = @stmt.step
      return nil if @stmt.done?
      row
    end


    def to_a
      []
    end
  end
end
