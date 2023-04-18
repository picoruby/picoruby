require "shell"
require "sqlite3"

shell = Shell.new
shell.setup_root_volume(:ram)
shell.setup_system_files

Dir.open "/" do |dir|
  while ent = dir.read
    p ent
  end
end

SQLite3::Database.new "/home/test.db" do |db|
  db.execute "CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT);"
  db.execute "INSERT INTO test (name) VALUES ('Mario');"
  db.execute "INSERT INTO test (name) VALUES ('Luigi');"
end

p File::Stat.new("/home/test.db").size

db = SQLite3::Database.new "/home/test.db"
db.execute "INSERT INTO test (name) VALUES ('Principessa Pesca');"
db.execute "SELECT * FROM test;" do |row|
  p row
end
db.close
