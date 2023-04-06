require "time"

class SQLite3
  class Database
    def execute(sql, &block)
      rows = _execute(sql)
      if block
        rows.each do |row|
          block.call(row)
        end
      else
        rows
      end
    end
  end
end
