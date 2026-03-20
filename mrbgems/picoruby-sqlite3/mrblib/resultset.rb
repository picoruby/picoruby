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
      rows = [] #: Array[untyped]
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
<<<<<<< HEAD
        row_hash = {} #: Hash[String, untyped]
        @stmt.columns&.each_with_index do |column, i|
          row_hash[column] = row[i]
=======
        row_hash = {}
        columns = @stmt.columns
        if columns
          ci = 0
          while ci < columns.size
            row_hash[columns[ci]] = row[ci]
            ci += 1
          end
>>>>>>> origin/master
        end
        row_hash
      else
        row
      end
    end
  end
end
