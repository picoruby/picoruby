class SQLite3
  # A focused subset of the sqlite3 gem's Pragmas, covering what is most useful
  # on a microcontroller: schema migration (user_version) and flash tuning
  # (journal_mode, synchronous, ...). Any other pragma is still reachable with
  # #pragma or a raw execute("PRAGMA ...").
  module Pragmas
    # Run a PRAGMA. With no value it queries and returns the rows; with a value
    # it sets and returns whatever rows the pragma emits (usually none).
    def pragma(name, value = nil)
      if value.nil?
        execute("PRAGMA #{name}")
      else
        execute("PRAGMA #{name} = #{pragma_value(value)}")
      end
    end

    # Schema version stamp. The usual lever for on-device migrations: bump it
    # after applying a migration and compare it on the next boot.
    def user_version
      int_pragma("user_version")
    end

    def user_version=(version)
      execute("PRAGMA user_version = #{version.to_i}")
    end

    # "delete", "truncate", "persist", "memory", "wal" or "off". "memory" and
    # "off" avoid journal writes to flash at the cost of crash safety.
    def journal_mode
      get_first_value("PRAGMA journal_mode").to_s
    end

    def journal_mode=(mode)
      execute("PRAGMA journal_mode = #{pragma_token(mode)}")
    end

    # 0 (off), 1 (normal), 2 (full) or 3 (extra). Lower values mean fewer flash
    # syncs but less durability across power loss.
    def synchronous
      int_pragma("synchronous")
    end

    def synchronous=(mode)
      execute("PRAGMA synchronous = #{pragma_token(mode)}")
    end

    def cache_size
      int_pragma("cache_size")
    end

    def cache_size=(size)
      execute("PRAGMA cache_size = #{size.to_i}")
    end

    def page_size
      int_pragma("page_size")
    end

    def page_size=(size)
      execute("PRAGMA page_size = #{size.to_i}")
    end

    # Foreign key enforcement is off by default in SQLite; turn it on per
    # connection when you rely on it.
    def foreign_keys
      get_first_value("PRAGMA foreign_keys") == 1
    end

    def foreign_keys=(enabled)
      execute("PRAGMA foreign_keys = #{enabled ? 'ON' : 'OFF'}")
    end

    def auto_vacuum
      int_pragma("auto_vacuum")
    end

    def auto_vacuum=(mode)
      execute("PRAGMA auto_vacuum = #{pragma_token(mode)}")
    end

    def freelist_count
      int_pragma("freelist_count")
    end

    # Returns "ok" when the database is intact, otherwise a description of the
    # first problem found. Useful after an unexpected power loss.
    def integrity_check
      get_first_value("PRAGMA integrity_check").to_s
    end

    private

    # A single-value pragma read back as an Integer. Going through to_s keeps
    # this total over every type SQLite may hand back (including nil).
    def int_pragma(name)
      get_first_value("PRAGMA #{name}").to_s.to_i
    end

    # A mode name (Symbol/String like :wal or "MEMORY") is a bare token, not a
    # quoted value.
    def pragma_token(value)
      value.to_s
    end

    def pragma_value(value)
      case value
      when Integer, Symbol
        value.to_s
      when true
        "ON"
      when false
        "OFF"
      else
        "'#{value.to_s.gsub("'", "''")}'"
      end
    end
  end

  class Database
    include Pragmas
  end
end
