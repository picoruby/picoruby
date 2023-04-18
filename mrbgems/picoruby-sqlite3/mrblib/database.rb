require "vfs"
require "time"

class SQLite3
  class Database
    class << self
      def new(filename)
        volume, path = VFS.sanitize_and_split(filename)
        SQLite3.vfs_methods = volume[:driver].class.vfs_methods
        db = SQLite3::Database._open("#{volume[:driver].prefix}#{path}")
        return db unless block_given?
        begin
          yield db
        ensure
          db.close
        end
        return db
      end
      alias :open :new
    end

    def execute(sql, *params, &block)
      rows = _execute(sql, params)
      return rows unless block
      case rows
      when Array
        rows.each do |row|
          block.call(row)
        end
      when Hash
        rows.each do |key, value|
          block.call(key, value)
        end
      end
    end
  end
end
