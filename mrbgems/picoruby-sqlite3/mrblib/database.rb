require "time"
begin
  require "vfs"
rescue LoadError
  # picoruby.wasm ships no filesystem VFS. There the database is memory backed
  # and persisted to IndexedDB; see the wasm branch in .new / #persist.
  # The POSIX host build has neither, and opens the host's files with
  # SQLite's unix VFS (_open_file).
  begin
    require "indexeddb"
    require "base64"
  rescue LoadError
  end
end

class SQLite3
  class Database
    class << self
      # new(filename, options = {}): the options Hash of the sqlite3 gem,
      # with or without braces (no keyword parameter here, so keywords
      # collect into it). :results_as_hash and :default_transaction_mode
      # are read; the others (:readonly, :flags, :strict, ...) have no
      # effect here.
      def new(filename, options = {})
        results_as_hash = options[:results_as_hash] || false
        default_transaction_mode = options[:default_transaction_mode] || :deferred
        db = if filename == ":memory:"
          # CRuby sqlite3 compatible: a private in-memory database bound to
          # no file and, on wasm, to no snapshot name. Nothing is restored on
          # open and #close persists nothing. Each ":memory:" handle is its
          # own independent database.
          mem = _open_memory
          # Keep sort/temp structures off the (driverless) VFS. The wasm
          # build fixes this at compile time (SQLITE_TEMP_STORE=3); other
          # targets get the runtime equivalent here.
          mem.execute("PRAGMA temp_store = 2") if defined?(VFS)
          mem
        elsif defined?(VFS)
          volume, path = VFS.sanitize_and_split(filename)
          # The driver prepends its own prefix, so every path SQLite derives
          # from this one (journals, temporary files) stays valid too
          _open(volume[:driver], path)
        elsif respond_to?(:_open_file, true)
          # The POSIX host build: the path names a file of the host, as it
          # does for File.open. SQLite makes a relative path absolute with
          # the current directory of the process.
          _open_file(filename)
        elsif defined?(IndexedDB)
          # No VFS (picoruby.wasm): open an in-memory database and restore any
          # snapshot previously persisted under this name.
          _open_memory._restore_from_store(filename)
        else
          raise SQLite3::CantOpenException.new(
            "unable to open database file: #{filename} (only \":memory:\" is available in this build)")
        end
        db.results_as_hash = results_as_hash
        db.default_transaction_mode = default_transaction_mode
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

      # The string with every ' doubled, for a SQL literal
      def quote(string)
        string.gsub("'", "''")
      end
    end

    attr_accessor :results_as_hash
    # The mode of a #transaction given none: :deferred, :immediate or
    # :exclusive
    attr_accessor :default_transaction_mode

    # The text encoding of the database, as a name. The build omits UTF-16
    # (SQLITE_OMIT_UTF16), and PRAGMA encoding with it, so it is UTF-8.
    def encoding
      "UTF-8"
    end

    # How long a statement waits on a locked database, in milliseconds
    def busy_timeout=(milliseconds)
      execute("PRAGMA busy_timeout = #{milliseconds.to_i}")
    end
    alias busy_timeout busy_timeout=
    alias busy_handler_timeout= busy_timeout=

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

    # Run a script of several statements, as #execute_batch does, and
    # return the rows of every query in it, each value a String (nil for
    # NULL) as sqlite3_exec() hands them out; a Hash per row when
    # results_as_hash. With a block, each row is yielded instead.
    def execute_batch2(sql)
      rows = [] #: Array[untyped]
      _exec_batch(sql, rows, results_as_hash)
      return rows unless block_given?
      i = 0
      while i < rows.size
        yield rows[i]
        i += 1
      end
      rows
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
    def transaction(mode = nil)
      mode ||= default_transaction_mode || :deferred
      execute("BEGIN #{mode.to_s.upcase} TRANSACTION")
      return true unless block_given?
      aborting = false
      begin
        yield self
      rescue => e
        aborting = true
        # Not a bare `raise`: on the mruby VM that would not re-raise but
        # raise a fresh empty RuntimeError, hiding the exception class
        # (e.g. SQLite3::Exception) from the caller.
        raise e
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

    if !defined?(VFS) && defined?(IndexedDB)
      # --- picoruby.wasm: snapshot persistence to IndexedDB ------------------
      #
      # The working database is in memory. Its bytes are snapshotted to
      # IndexedDB (keyed by the name passed to .new) at await points only:
      # restored on open, saved by #persist and automatically on #close. The GC
      # finalizer never persists (it cannot run Ruby / await from a free func).
      # Bytes are base64 wrapped so they survive the JS bridge and the store
      # intact. Note: writes since the last persist are lost on a crash/reload.
      STORE_NAME = "sqlite3".freeze

      def _restore_from_store(name)
        @db_name = name
        snapshot = self.class.__store[name]
        deserialize(Base64.decode64(snapshot)) if snapshot
        self
      end

      # Snapshot the current database into IndexedDB under its name.
      def persist
        name = @db_name
        unless name
          raise SQLite3::Exception.new(
            "cannot persist a database that has no snapshot name (\":memory:\")")
        end
        self.class.__store[name] = Base64.strict_encode64(serialize)
        true
      end

      alias __close_without_persist close
      def close
        begin
          persist if @db_name && !closed?
        ensure
          __close_without_persist
        end
      end

      class << self
        # One IndexedDB KV store shared by every database, keyed by db name.
        def __store
          @__store ||= IndexedDB::KVS.open(STORE_NAME)
        end
      end
    end
  end
end
