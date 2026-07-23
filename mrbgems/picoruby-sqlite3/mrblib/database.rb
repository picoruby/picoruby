require "vfs"
require "time"

class SQLite3
  class Database
    class << self
      def new(filename, results_as_hash: false)
        volume, path = VFS.sanitize_and_split(filename)
        # The driver prepends its own prefix, so every path SQLite derives
        # from this one (journals, temporary files) stays valid too
        db = _open(volume[:driver], path)
        db.results_as_hash = results_as_hash
        if block_given?
          begin
            yield db
          ensure
            db.close
          end
        end
        return db
      end
      alias :open :new
    end

    attr_accessor :results_as_hash

    def execute(sql, bind_vars = [])
      prepare(sql) do |stmt|
        stmt.bind_params(*bind_vars)
        resultset = SQLite3::ResultSet.new(self, stmt)
        if block_given?
          row = resultset.next
          while row
            yield row
            row = resultset.next
          end
        else
          resultset.to_a
        end
      end
    end

    def prepare(sql)
      stmt = SQLite3::Statement.new(self, sql)
      return stmt unless block_given?
      begin
        yield stmt
      ensure
        stmt.close unless stmt.closed?
      end
    end
  end
end
