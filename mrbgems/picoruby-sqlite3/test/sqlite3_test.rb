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
end
