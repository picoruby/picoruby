# The test file is also `load`ed on CRuby to discover the test classes, where
# "littlefs" does not exist. LoadError is a ScriptError, not a StandardError, so
# the runner's discovery rescue does not catch it; guard the require here. On the
# target VM (PicoRuby) littlefs is built in and this require succeeds.
begin
  require "littlefs"
rescue LoadError
end

# picoruby-sqlite3 runs on top of picoruby-vfs, so the tests mount a RAM backed
# littlefs volume (ports/posix) at "/" and let SQLite talk to it through the VFS
# bridge. No board is needed. Every test method reuses the same mount (mounting
# twice at "/" would raise), and isolates itself with its own database file plus
# a DROP TABLE IF EXISTS, since the RAM device persists for the process lifetime.
class Sqlite3Test < Picotest::Test
  def setup
    skip "Not supported on FemtoRuby" if femtoruby?
    # On picoruby.wasm the database is memory backed (no VFS to mount).
    return if wasm?
    unless VFS::VOLUMES.any? { |v| v[:mountpoint] == "/" }
      # Format the RAM device before mounting. A fresh device is unformatted, so
      # mounting it would make littlefs print a "Corrupted dir pair" trace to
      # stdout on its way to auto-formatting, which corrupts the Picotest result
      # JSON the runner parses from stdout.
      fs = Littlefs.new(:flash, label: "SQLITE3")
      fs.mkfs
      VFS.mount(fs, "/")
    end
  end

  # Reset a table on a per-test database file so tests do not see each other's
  # rows even though the RAM volume outlives a single test method.
  def fresh_db(path)
    db = SQLite3::Database.new(path)
    db.execute("DROP TABLE IF EXISTS users;")
    db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);")
    db
  end

  def test_sqlite_version
    SQLite3::Database.new("/version.db") do |db|
      rows = db.execute("SELECT sqlite_version();")
      assert_equal(1, rows.size)
      assert_not_nil(rows[0][0])
      # 3.53.0300 amalgamation, so the string starts with "3."
      assert_equal("3.", rows[0][0][0, 2])
    end
  end

  def test_block_form_closes_database
    db = SQLite3::Database.new("/block.db") { |d| d }
    assert(db.closed?)
  end

  def test_create_insert_and_select_as_array
    db = fresh_db("/array.db")
    db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Alice", 30])
    db.execute("INSERT INTO users VALUES (?, ?, ?)", [2, "Bob", 25])
    rows = db.execute("SELECT id, name, age FROM users ORDER BY id;")
    assert_equal([[1, "Alice", 30], [2, "Bob", 25]], rows)
    db.close
  end

  def test_execute_with_block_iterates_rows
    db = fresh_db("/each.db")
    db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Alice", 30])
    db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Bob", 25])
    names = []
    db.execute("SELECT name FROM users ORDER BY id;") do |row|
      names << row[0]
    end
    assert_equal(["Alice", "Bob"], names)
    db.close
  end

  def test_bind_by_name
    db = fresh_db("/named.db")
    db.execute("INSERT INTO users (name, age) VALUES (:name, :age)", [{ name: "Carol", age: 41 }])
    rows = db.execute("SELECT name, age FROM users WHERE name = :name;", [{ name: "Carol" }])
    assert_equal([["Carol", 41]], rows)
    db.close
  end

  def test_results_as_hash
    db = fresh_db("/hash.db")
    db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Alice", 30])
    db.results_as_hash = true
    row = db.execute("SELECT id, name, age FROM users;")[0]
    assert_equal(1, row["id"])
    assert_equal("Alice", row["name"])
    assert_equal(30, row["age"])
    db.close
  end

  def test_results_as_hash_option_on_new
    SQLite3::Database.new("/hashopt.db", results_as_hash: true) do |db|
      db.execute("DROP TABLE IF EXISTS users;")
      db.execute("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER);")
      db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Bob", 25])
      row = db.execute("SELECT name FROM users;")[0]
      assert_equal("Bob", row["name"])
    end
  end

  def test_prepared_statement_reuse
    db = fresh_db("/prepared.db")
    stmt = db.prepare("INSERT INTO users (name) VALUES (?);")
    names = %w[Mario Luigi Peach]
    i = 0
    while i < names.size
      stmt.execute(names[i])
      i += 1
    end
    stmt.close
    assert(stmt.closed?)
    rows = db.execute("SELECT name FROM users ORDER BY id;")
    assert_equal([["Mario"], ["Luigi"], ["Peach"]], rows)
    db.close
  end

  def test_statement_columns_and_types
    db = fresh_db("/meta.db")
    db.prepare("SELECT id, name, age FROM users;") do |stmt|
      assert_equal(["id", "name", "age"], stmt.columns)
      assert_equal(["INTEGER", "TEXT", "INTEGER"], stmt.types)
    end
    db.close
  end

  def test_update_and_delete
    db = fresh_db("/mutate.db")
    names = %w[Toad Koopa Kuribo]
    i = 0
    while i < names.size
      db.execute("INSERT INTO users (name) VALUES (?);", [names[i]])
      i += 1
    end
    db.execute("UPDATE users SET name = ? WHERE name = ?;", ["Bowser", "Koopa"])
    db.execute("DELETE FROM users WHERE name = ?;", ["Kuribo"])
    rows = db.execute("SELECT COUNT(*), MAX(name) FROM users;")
    assert_equal([[2, "Toad"]], rows)
    db.close
  end

  def test_type_round_trip
    db = SQLite3::Database.new("/types.db")
    db.execute("DROP TABLE IF EXISTS t;")
    db.execute("CREATE TABLE t (i INTEGER, f REAL, s TEXT, n INTEGER, b BLOB);")
    db.execute("INSERT INTO t VALUES (?, ?, ?, ?, ?)", [42, 3.5, "hello", nil, "world"])
    row = db.execute("SELECT i, f, s, n, b FROM t;")[0]
    assert_equal(42, row[0])
    assert_in_delta(3.5, row[1])
    assert_equal("hello", row[2])
    assert_nil(row[3])
    assert_equal("world", row[4])
    db.close
  end

  def test_boolean_binds_as_integer
    db = SQLite3::Database.new("/bool.db")
    db.execute("DROP TABLE IF EXISTS flags;")
    db.execute("CREATE TABLE flags (on_flag INTEGER, off_flag INTEGER);")
    db.execute("INSERT INTO flags VALUES (?, ?)", [true, false])
    row = db.execute("SELECT on_flag, off_flag FROM flags;")[0]
    assert_equal(1, row[0])
    assert_equal(0, row[1])
    db.close
  end

  def test_persistence_across_reopen
    # On wasm this rides the auto-persist-on-close snapshot to IndexedDB; on
    # MCU/POSIX it is real file persistence. Either way, reopening sees the data.
    db = fresh_db("/persist.db")
    db.execute("INSERT INTO users (name) VALUES (?);", ["Daisy"])
    db.close
    SQLite3::Database.new("/persist.db") do |reopened|
      rows = reopened.execute("SELECT name FROM users;")
      assert_equal([["Daisy"]], rows)
    end
  end

  def test_invalid_sql_raises
    SQLite3::Database.new("/error.db") do |db|
      assert_raise(SQLite3::Exception) do
        db.execute("SELECT * FROM no_such_table;")
      end
    end
  end

  def test_constraint_violation_raises_subclass
    db = fresh_db("/constraint.db")
    db.execute("INSERT INTO users (id, name) VALUES (1, 'Alice')")
    # Reusing the primary key violates the UNIQUE constraint
    assert_raise(SQLite3::ConstraintException) do
      db.execute("INSERT INTO users (id, name) VALUES (1, 'Bob')")
    end
    # The subclass is still a SQLite3::Exception for generic rescues
    assert_true(SQLite3::ConstraintException.ancestors.include?(SQLite3::Exception))
    db.close
  end

  def test_last_insert_row_id
    db = fresh_db("/rowid.db")
    db.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    assert_equal(1, db.last_insert_row_id)
    db.execute("INSERT INTO users (name) VALUES (?)", ["Bob"])
    assert_equal(2, db.last_insert_row_id)
    db.close
  end

  def test_changes_and_total_changes
    db = fresh_db("/changes.db")
    db.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    db.execute("INSERT INTO users (name) VALUES (?)", ["Bob"])
    db.execute("UPDATE users SET age = 1")
    assert_equal(2, db.changes)
    # 1 (Alice) + 1 (Bob) + 2 (UPDATE touches both rows)
    assert_equal(4, db.total_changes)
    db.close
  end

  def test_execute_batch
    db = SQLite3::Database.new("/batch.db")
    db.execute_batch(<<~SQL)
      DROP TABLE IF EXISTS a;
      CREATE TABLE a (id INTEGER);
      INSERT INTO a VALUES (1);
      INSERT INTO a VALUES (2);
    SQL
    assert_equal([[1], [2]], db.execute("SELECT id FROM a ORDER BY id"))
    db.close
  end

  def test_execute_batch_error_raises
    db = SQLite3::Database.new("/batch_err.db")
    assert_raise(SQLite3::Exception) do
      db.execute_batch("CREATE TABLE ok (id INTEGER); INVALID SQL HERE;")
    end
    db.close
  end

  def test_get_first_row
    db = fresh_db("/first_row.db")
    db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Alice", 30])
    db.execute("INSERT INTO users (name, age) VALUES (?, ?)", ["Bob", 25])
    assert_equal([1, "Alice", 30], db.get_first_row("SELECT id, name, age FROM users ORDER BY id"))
    assert_nil(db.get_first_row("SELECT * FROM users WHERE name = ?", ["Nobody"]))
    db.close
  end

  def test_get_first_value
    db = fresh_db("/first_value.db")
    db.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    db.execute("INSERT INTO users (name) VALUES (?)", ["Bob"])
    assert_equal(2, db.get_first_value("SELECT COUNT(*) FROM users"))
    assert_equal("Alice", db.get_first_value("SELECT name FROM users ORDER BY id"))
    assert_nil(db.get_first_value("SELECT name FROM users WHERE name = ?", ["Nobody"]))
    db.close
  end

  def test_query_without_block
    db = fresh_db("/query.db")
    db.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    result = db.query("SELECT name FROM users")
    assert_equal(["Alice"], result.next)
    assert_nil(result.next)
    result.close
    db.close
  end

  def test_query_with_block_closes
    db = fresh_db("/query_block.db")
    db.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    captured = nil
    db.query("SELECT name FROM users") do |result|
      captured = result
      assert_equal(["Alice"], result.next)
    end
    assert_true(captured.closed?)
    db.close
  end

  def test_transaction_active
    db = fresh_db("/txn_active.db")
    assert_false(db.transaction_active?)
    db.transaction
    assert_true(db.transaction_active?)
    db.commit
    assert_false(db.transaction_active?)
    db.close
  end

  def test_transaction_block_commits
    db = fresh_db("/txn_commit.db")
    db.transaction do |t|
      t.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
      t.execute("INSERT INTO users (name) VALUES (?)", ["Bob"])
    end
    assert_false(db.transaction_active?)
    assert_equal(2, db.get_first_value("SELECT COUNT(*) FROM users"))
    db.close
  end

  def test_transaction_block_rolls_back_on_error
    db = fresh_db("/txn_rollback.db")
    db.execute("INSERT INTO users (name) VALUES (?)", ["Seed"])
    assert_raise(RuntimeError) do
      db.transaction do |t|
        t.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
        raise "boom"
      end
    end
    assert_false(db.transaction_active?)
    # Only the pre-transaction row survives
    assert_equal(1, db.get_first_value("SELECT COUNT(*) FROM users"))
    db.close
  end

  def test_manual_rollback
    db = fresh_db("/manual_rollback.db")
    db.transaction
    db.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    db.rollback
    assert_equal(0, db.get_first_value("SELECT COUNT(*) FROM users"))
    db.close
  end

  def test_backup_copies_database
    src = fresh_db("/backup_src.db")
    src.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    src.execute("INSERT INTO users (name) VALUES (?)", ["Bob"])

    dst = SQLite3::Database.new("/backup_dst.db")
    src.backup(dst)
    src.close

    assert_equal([[1, "Alice", nil], [2, "Bob", nil]],
      dst.execute("SELECT id, name, age FROM users ORDER BY id"))
    dst.close
  end

  def test_backup_class_step_reports_done
    src = fresh_db("/backup_step_src.db")
    src.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    dst = SQLite3::Database.new("/backup_step_dst.db")

    b = SQLite3::Backup.new(dst, "main", src, "main")
    assert_true(b.pagecount >= 0)
    status = b.step(-1)
    assert_equal(SQLite3::Backup::DONE, status)
    assert_equal(0, b.remaining)
    b.finish
    assert_equal("Alice", dst.get_first_value("SELECT name FROM users"))
    src.close
    dst.close
  end

  def test_closed_database_guard
    db = fresh_db("/closed_guard.db")
    db.close
    assert_raise(SQLite3::Exception) { db.last_insert_row_id }
    assert_raise(SQLite3::Exception) { db.changes }
    assert_raise(SQLite3::Exception) { db.execute_batch("SELECT 1") }
  end

  def test_user_version
    db = SQLite3::Database.new("/uv.db")
    assert_equal(0, db.user_version)
    db.user_version = 3
    assert_equal(3, db.user_version)
    db.close # on wasm, close auto-persists the snapshot
    # Persists with the database (file on MCU, IndexedDB snapshot on wasm)
    SQLite3::Database.new("/uv.db") do |reopened|
      assert_equal(3, reopened.user_version)
    end
  end

  def test_journal_mode
    db = SQLite3::Database.new("/journal.db")
    db.journal_mode = :memory
    assert_equal("memory", db.journal_mode)
    db.close
  end

  def test_synchronous
    db = SQLite3::Database.new("/sync.db")
    db.synchronous = 0
    assert_equal(0, db.synchronous)
    db.synchronous = :full
    assert_equal(2, db.synchronous)
    db.close
  end

  def test_page_and_cache_size
    db = SQLite3::Database.new("/sizes.db")
    assert(db.page_size >= 512)
    db.cache_size = 200
    assert_equal(200, db.cache_size)
    db.close
  end

  def test_foreign_keys
    db = SQLite3::Database.new("/fk.db")
    assert_false(db.foreign_keys)
    db.foreign_keys = true
    assert_true(db.foreign_keys)
    db.foreign_keys = false
    assert_false(db.foreign_keys)
    db.close
  end

  def test_integrity_check
    db = fresh_db("/integrity.db")
    db.execute("INSERT INTO users (name) VALUES (?)", ["Alice"])
    assert_equal("ok", db.integrity_check)
    db.close
  end

  def test_generic_pragma
    db = SQLite3::Database.new("/generic_pragma.db")
    db.pragma("user_version", 7)
    assert_equal([[7]], db.pragma("user_version"))
    db.close
  end

  def test_readonly_and_filename
    db = SQLite3::Database.new("/meta_db.db")
    assert_false(db.readonly?)
    if wasm?
      # An in-memory database has no filename
      assert_nil(db.filename)
    else
      # sqlite reports the (VFS relative) path it was opened with
      assert_true(db.filename.include?("meta_db.db"))
    end
    db.close
  end

  # ---- wasm-only: in-memory DB + IndexedDB snapshot persistence ----

  def test_wasm_explicit_persist_and_restore
    skip "wasm only" unless wasm?
    db = SQLite3::Database.new("explicit_persist")
    db.execute("CREATE TABLE t (id INTEGER PRIMARY KEY, v TEXT)")
    db.execute("INSERT INTO t (v) VALUES (?)", ["hello"])
    db.persist
    # The live handle is untouched by persist
    assert_equal([[1, "hello"]], db.execute("SELECT id, v FROM t"))
    # A second handle by the same name restores what #persist just wrote (the
    # first handle is still open, so only the explicit persist could have saved)
    reopened = SQLite3::Database.new("explicit_persist")
    assert_equal([[1, "hello"]], reopened.execute("SELECT id, v FROM t"))
    reopened.close
    db.close
  end

  def test_wasm_serialize_round_trip
    skip "wasm only" unless wasm?
    db = SQLite3::Database.new("serial_rt")
    db.execute("CREATE TABLE t (v TEXT)")
    db.execute("INSERT INTO t (v) VALUES (?)", ["roundtrip"])
    bytes = db.serialize
    assert(bytes.is_a?(String))
    assert(bytes.length > 0)
    db.close

    other = SQLite3::Database.new("serial_rt_other")
    other.deserialize(bytes)
    assert_equal([["roundtrip"]], other.execute("SELECT v FROM t"))
    other.close
  end

  def test_wasm_fresh_name_starts_empty
    skip "wasm only" unless wasm?
    db = SQLite3::Database.new("never_seen_#{object_id}")
    rows = db.execute("SELECT name FROM sqlite_master WHERE type='table'")
    assert_equal([], rows)
    db.close
  end
end
