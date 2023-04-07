require "time"

class SQLite3
  class Database
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
