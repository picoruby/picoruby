require "time"
begin
  require "vfs"
rescue LoadError
  # picoruby.wasm ships no filesystem VFS. There the database is memory backed
  # and persisted to IndexedDB; see the wasm branch in .new / #persist.
end

class SQLite3
  class Database
    class << self
      def new(filename, results_as_hash: false)
        db = if defined?(VFS)
          volume, path = VFS.sanitize_and_split(filename)
          # The driver prepends its own prefix, so every path SQLite derives
          # from this one (journals, temporary files) stays valid too
          _open(volume[:driver], path)
        else
          # No VFS (picoruby.wasm): open an in-memory database.
          _open_memory
        end
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

    # Like #execute but returns a ResultSet you step through yourself. With a
    # block the ResultSet is yielded and closed for you.
    def query(sql, bind_vars = [])
      stmt = prepare(sql)
      stmt.bind_params(*bind_vars) unless bind_vars.empty?
      result = SQLite3::ResultSet.new(self, stmt)
      return result unless block_given?
      begin
        yield result
      ensure
        result.close
      end
    end

    def get_first_row(sql, bind_vars = [])
      execute(sql, bind_vars).first
    end

    def get_first_value(sql, bind_vars = [])
      execute(sql, bind_vars) do |row|
        return row.is_a?(Array) ? row[0] : row.values.first
      end
      nil
    end

    # Wrap a block in a transaction. Committing on success and rolling back on
    # an exception. On flash backed storage this is the main lever for cutting
    # write wear: many INSERTs in one transaction become a single commit.
    def transaction(mode = :deferred)
      execute("BEGIN #{mode.to_s.upcase} TRANSACTION")
      return true unless block_given?
      aborting = false
      begin
        yield self
      rescue
        aborting = true
        raise
      ensure
        aborting ? rollback : commit
      end
    end

    def commit
      execute("COMMIT TRANSACTION")
      true
    end

    def rollback
      execute("ROLLBACK TRANSACTION")
      true
    end

    # Copy this database into an already open destination database. Passing -1
    # to Backup#step copies every remaining page, so one step finishes the copy.
    def backup(dst, srcname: "main", dstname: "main")
      b = SQLite3::Backup.new(dst, dstname, self, srcname)
      begin
        b.step(-1)
      ensure
        b.finish
      end
      true
    end
  end
end
