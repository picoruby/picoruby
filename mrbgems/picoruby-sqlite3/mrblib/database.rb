require "vfs"
require "time"

class SQLite3
  class Database
    class << self
      def new(filename)
        volume, path = VFS.sanitize_and_split(filename)
        SQLite3.vfs_methods = volume[:driver].class.vfs_methods
        db = SQLite3::Database._open("#{volume[:driver].prefix}#{path}")
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

    def execute(sql, bind_vars = [])
      prepare(sql) do |stmt|
        stmt.bind_params(*bind_vars)
        resultset = SQLite3::ResultSet.new(self, stmt)
        if block_given?
          resultset.each do |row|
            yield row
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
