require "shell"
require "sqlite3"

Shell.setup(:ram)

Dir.open "/" do |dir|
  while ent = dir.read
    p ent
  end
end

SQLite3.vfs_methods = FAT.vfs_methods
db = SQLite3::Database.open "/home/test.db"
db.execute "CREATE TABLE test (id INTEGER PRIMARY KEY, name TEXT);"
db.execute "INSERT INTO test (name) VALUES ('hello');"
db.execute "INSERT INTO test (name) VALUES ('ruby');"
db.execute "SELECT * FROM test;" do |row|
  p row
end
db.close

p File::Stat.new("/home/test.db").size
