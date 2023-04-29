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

    def to_a
      rows = []
      while row = self.next
        rows << row
      end
      rows
    end

    def each
      while node = self.next
        yield node
      end
    end

    def next
      row = @stmt.step
      return nil if @stmt.done? || row.nil?
      if @db.results_as_hash
        row_hash = {}
        @stmt.columns&.each_with_index do |column, i|
          row_hash[column] = row[i]
        end
        row_hash
      else
        row
      end
    end
  end
end
