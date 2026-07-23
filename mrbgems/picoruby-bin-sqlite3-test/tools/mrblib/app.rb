require "littlefs"
require "sqlite3"
require "picorubyvm"

# An uncaught exception just kills the task without a word, so report it here
begin

  VFS.mount(Littlefs.new(:flash, label: "SQLITE3"), "/")
  Dir.mkdir("/home") unless Dir.exist?("/home")

  puts "picoruby-sqlite3 on picoruby-littlefs"
  puts PicoRubyVM.memory_statistics

  db = SQLite3::Database.new "/home/test.db"

  db.execute("SELECT sqlite_version();") do |row|
    puts "sqlite_version=#{row[0]}"
  end

  db.execute("CREATE TABLE IF NOT EXISTS test
    (
      id INTEGER PRIMARY KEY,
      name TEXT,
      created_at TEXT DEFAULT (STRFTIME('%Y-%m-%d %H:%M:%f', 'NOW'))
    );")

  print "journal_mode="
  db.execute("PRAGMA journal_mode;") do |row|
    puts row[0]
  end

  stmt = db.prepare "INSERT INTO test (name) VALUES (?);"
  names = %w[Mario Luigi Peach Toad Koopa Kuribo]
  i = 0
  while i < names.size
    stmt.execute names[i]
    i += 1
  end
  stmt.close

  puts "--- rows as Array ---"
  db.execute("SELECT * FROM test;") do |row|
    p row
  end

  db.results_as_hash = true
  puts "--- rows as Hash ---"
  db.execute("SELECT * FROM test;") do |row|
    p row
  end
  db.results_as_hash = false

  puts "--- bind by name ---"
  db.execute("SELECT name FROM test WHERE name = :name;", [{ name: "Peach" }]) do |row|
    p row
  end

  puts PicoRubyVM.memory_statistics
  db.close
  p File.exist?("/home/test.db")

  # Reopening exercises the "existing file" path of the VFS bridge
  puts "--- reopen ---"
  SQLite3::Database.new("/home/test.db") do |reopened|
    reopened.execute("UPDATE test SET name = ? WHERE name = ?;", ["Bowser", "Koopa"])
    reopened.execute("DELETE FROM test WHERE name = ?;", ["Kuribo"])
    reopened.execute("SELECT COUNT(*), MAX(name) FROM test;") do |row|
      p row
    end
  end

  puts "done"

rescue => e
  puts "FAILED: #{e.class}: #{e.message}"
end
